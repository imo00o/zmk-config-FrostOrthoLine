/*
 * PAT9125EL を X/Y の相対移動入力（カーソル）として使うドライバ
 *
 * レジスタ操作は Zephyr 標準の input_pat912x.c と同じ。
 * Zephyr 標準ドライバは MOTION をエッジ割り込みで検出するため、エッジを
 * 取りこぼすと MOTION が出たままになり二度と読まなくなることがある。
 * ここでは zmk-driver-hires-dial と同様に、
 *   - 初期化時のソフトウェアリセットはしない
 *   - MOTION はレベル割り込みで検出し、出ている間は一定間隔で読み続ける
 * ようにしている。
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT pixart_pat9125_zmk

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(pat9125_zmk, CONFIG_INPUT_LOG_LEVEL);

#define PAT9125_PRODUCT_ID1 0x00
#define PAT9125_MOTION_STATUS 0x02
#define PAT9125_DELTA_X_LO 0x03
#define PAT9125_OPERATION_MODE 0x05
#define PAT9125_CONFIGURATION 0x06
#define PAT9125_RES_X 0x0d
#define PAT9125_RES_Y 0x0e
#define PAT9125_DELTA_XY_HI 0x12

#define PRODUCT_ID_PAT9125EL 0x3191

#define CONFIGURATION_CLEAR 0x17
#define CONFIGURATION_PD_ENH BIT(3)
#define MOTION_STATUS_MOTION BIT(7)
#define OPERATION_MODE_SLEEP_1_EN BIT(4)
#define OPERATION_MODE_SLEEP_12_EN (BIT(4) | BIT(3))
#define RES_SCALING_FACTOR 5
#define RES_MAX (UINT8_MAX * RES_SCALING_FACTOR)

#define INIT_DELAY_MS 2
#define ID_READ_RETRIES 5
#define ID_READ_RETRY_DELAY_MS 10

struct pat9125_config {
    struct i2c_dt_spec i2c;
    struct gpio_dt_spec motion_gpio;
    uint16_t res_x_cpi;
    uint16_t res_y_cpi;
    uint16_t motion_read_interval_ms;
    bool invert_x;
    bool invert_y;
    bool swap_xy;
    bool sleep1_enable;
    bool sleep2_enable;
};

struct pat9125_data {
    const struct device *dev;
    struct k_work_delayable motion_work;
    struct gpio_callback motion_cb;
};

static inline int32_t sign_extend_12(uint32_t value) { return (int32_t)(value << 20) >> 20; }

static int pat9125_configure(const struct device *dev) {
    const struct pat9125_config *cfg = dev->config;
    uint8_t id[2];
    int err = -EIO;

    /* 電源投入直後は応答しないことがあるので数回リトライする */
    for (int i = 0; i < ID_READ_RETRIES; i++) {
        err = i2c_burst_read_dt(&cfg->i2c, PAT9125_PRODUCT_ID1, id, sizeof(id));
        if (err == 0) {
            break;
        }
        k_sleep(K_MSEC(ID_READ_RETRY_DELAY_MS));
    }
    if (err < 0) {
        LOG_ERR("Product ID read failed: %d", err);
        return err;
    }

    if (sys_get_be16(id) != PRODUCT_ID_PAT9125EL) {
        LOG_ERR("Invalid product id: %04x", sys_get_be16(id));
        return -ENOTSUP;
    }

    k_sleep(K_MSEC(INIT_DELAY_MS));

    err = i2c_reg_write_byte_dt(&cfg->i2c, PAT9125_CONFIGURATION, CONFIGURATION_CLEAR);
    if (err < 0) {
        return err;
    }

    err = i2c_reg_write_byte_dt(&cfg->i2c, PAT9125_RES_X, cfg->res_x_cpi / RES_SCALING_FACTOR);
    if (err < 0) {
        return err;
    }

    err = i2c_reg_write_byte_dt(&cfg->i2c, PAT9125_RES_Y, cfg->res_y_cpi / RES_SCALING_FACTOR);
    if (err < 0) {
        return err;
    }

    uint8_t sleep_bits = 0;
    if (cfg->sleep1_enable) {
        sleep_bits = cfg->sleep2_enable ? OPERATION_MODE_SLEEP_12_EN : OPERATION_MODE_SLEEP_1_EN;
    }

    return i2c_reg_update_byte_dt(&cfg->i2c, PAT9125_OPERATION_MODE, OPERATION_MODE_SLEEP_12_EN,
                                  sleep_bits);
}

/* 戻り値: 1 = 移動量あり, 0 = 移動なし, 負 = エラー */
static int pat9125_read_motion(const struct device *dev, int32_t *x, int32_t *y) {
    const struct pat9125_config *cfg = dev->config;
    uint8_t status;

    int err = i2c_reg_read_byte_dt(&cfg->i2c, PAT9125_MOTION_STATUS, &status);
    if (err < 0) {
        return err;
    }
    if (!(status & MOTION_STATUS_MOTION)) {
        return 0;
    }

    uint8_t xy[2];
    err = i2c_burst_read_dt(&cfg->i2c, PAT9125_DELTA_X_LO, xy, sizeof(xy));
    if (err < 0) {
        return err;
    }

    uint8_t high;
    err = i2c_reg_read_byte_dt(&cfg->i2c, PAT9125_DELTA_XY_HI, &high);
    if (err < 0) {
        return err;
    }

    *x = sign_extend_12(xy[0] | ((high << 4) & 0xf00));
    *y = sign_extend_12(xy[1] | ((high << 8) & 0xf00));
    return 1;
}

static int pat9125_interrupt_enable(const struct device *dev) {
    const struct pat9125_config *cfg = dev->config;

    return gpio_pin_interrupt_configure_dt(&cfg->motion_gpio, GPIO_INT_LEVEL_ACTIVE);
}

static int pat9125_interrupt_disable(const struct device *dev) {
    const struct pat9125_config *cfg = dev->config;

    return gpio_pin_interrupt_configure_dt(&cfg->motion_gpio, GPIO_INT_DISABLE);
}

static void pat9125_motion_work_handler(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct pat9125_data *data = CONTAINER_OF(dwork, struct pat9125_data, motion_work);
    const struct device *dev = data->dev;
    const struct pat9125_config *cfg = dev->config;

    int motion_active = gpio_pin_get_dt(&cfg->motion_gpio);
    if (motion_active < 0) {
        LOG_ERR("MOTION pin read failed: %d", motion_active);
        k_work_reschedule(&data->motion_work, K_MSEC(cfg->motion_read_interval_ms));
        return;
    }

    if (motion_active == 0) {
        /* MOTION が出ていないので、次に出るまでレベル割り込みで待つ */
        int err = pat9125_interrupt_enable(dev);
        if (err < 0) {
            LOG_ERR("MOTION interrupt enable failed: %d", err);
        }
        return;
    }

    int32_t x = 0;
    int32_t y = 0;
    int err = pat9125_read_motion(dev, &x, &y);
    if (err < 0) {
        LOG_ERR("Motion read failed: %d", err);
    } else if (err > 0 && (x != 0 || y != 0)) {
        if (cfg->swap_xy) {
            int32_t tmp = x;
            x = y;
            y = tmp;
        }
        if (cfg->invert_x) {
            x = -x;
        }
        if (cfg->invert_y) {
            y = -y;
        }

        LOG_DBG("x=%4d y=%4d", x, y);

        input_report_rel(dev, INPUT_REL_X, x, false, K_FOREVER);
        input_report_rel(dev, INPUT_REL_Y, y, true, K_FOREVER);
    }

    /* 割り込みは無効のまま一定間隔後にもう一度確認する。
     * MOTION がまだ出ていれば読み、出ていなければレベル割り込みを再開する。 */
    k_work_reschedule(&data->motion_work, K_MSEC(cfg->motion_read_interval_ms));
}

static void pat9125_motion_gpio_handler(const struct device *gpio_dev, struct gpio_callback *cb,
                                        uint32_t pins) {
    struct pat9125_data *data = CONTAINER_OF(cb, struct pat9125_data, motion_cb);

    ARG_UNUSED(gpio_dev);
    ARG_UNUSED(pins);

    /* レベル割り込みは出続けるので、ワーク側で読むまで止めておく */
    int err = pat9125_interrupt_disable(data->dev);
    if (err < 0) {
        LOG_ERR("MOTION interrupt disable failed: %d", err);
        return;
    }
    k_work_reschedule(&data->motion_work, K_NO_WAIT);
}

static int pat9125_init(const struct device *dev) {
    const struct pat9125_config *cfg = dev->config;
    struct pat9125_data *data = dev->data;
    int err;

    if (!i2c_is_ready_dt(&cfg->i2c)) {
        LOG_ERR("%s is not ready", cfg->i2c.bus->name);
        return -ENODEV;
    }
    if (!gpio_is_ready_dt(&cfg->motion_gpio)) {
        LOG_ERR("%s is not ready", cfg->motion_gpio.port->name);
        return -ENODEV;
    }

    data->dev = dev;
    k_work_init_delayable(&data->motion_work, pat9125_motion_work_handler);

    err = gpio_pin_configure_dt(&cfg->motion_gpio, GPIO_INPUT);
    if (err < 0) {
        LOG_ERR("MOTION pin configuration failed: %d", err);
        return err;
    }

    gpio_init_callback(&data->motion_cb, pat9125_motion_gpio_handler, BIT(cfg->motion_gpio.pin));
    err = gpio_add_callback_dt(&cfg->motion_gpio, &data->motion_cb);
    if (err < 0) {
        LOG_ERR("Could not set MOTION callback: %d", err);
        return err;
    }

    err = pat9125_configure(dev);
    if (err < 0) {
        LOG_ERR("Device configuration failed: %d", err);
        return err;
    }

    /* MOTION がすでに出ていても取りこぼさないよう、最初に一度確認する */
    k_work_reschedule(&data->motion_work, K_NO_WAIT);

    LOG_INF("PAT9125 ready (MOTION pin %u)", cfg->motion_gpio.pin);
    return 0;
}

#ifdef CONFIG_PM_DEVICE
static int pat9125_pm_action(const struct device *dev, enum pm_device_action action) {
    const struct pat9125_config *cfg = dev->config;
    struct pat9125_data *data = dev->data;
    int err;

    switch (action) {
    case PM_DEVICE_ACTION_SUSPEND:
        pat9125_interrupt_disable(dev);
        k_work_cancel_delayable(&data->motion_work);
        return i2c_reg_update_byte_dt(&cfg->i2c, PAT9125_CONFIGURATION, CONFIGURATION_PD_ENH,
                                      CONFIGURATION_PD_ENH);
    case PM_DEVICE_ACTION_RESUME:
        err = i2c_reg_update_byte_dt(&cfg->i2c, PAT9125_CONFIGURATION, CONFIGURATION_PD_ENH, 0);
        if (err < 0) {
            return err;
        }
        k_work_reschedule(&data->motion_work, K_NO_WAIT);
        return 0;
    default:
        return -ENOTSUP;
    }
}
#endif

#define PAT9125_INIT(n)                                                                            \
    BUILD_ASSERT(DT_INST_PROP(n, res_x_cpi) <= RES_MAX, "invalid res-x-cpi");                      \
    BUILD_ASSERT(DT_INST_PROP(n, res_y_cpi) <= RES_MAX, "invalid res-y-cpi");                      \
    BUILD_ASSERT(DT_INST_PROP(n, motion_read_interval_ms) > 0, "invalid motion-read-interval-ms"); \
    BUILD_ASSERT(DT_INST_PROP(n, sleep1_enable) || !DT_INST_PROP(n, sleep2_enable),                \
                 "sleep2-enable requires sleep1-enable");                                          \
                                                                                                   \
    static const struct pat9125_config pat9125_cfg_##n = {                                         \
        .i2c = I2C_DT_SPEC_INST_GET(n),                                                            \
        .motion_gpio = GPIO_DT_SPEC_INST_GET(n, motion_gpios),                                     \
        .res_x_cpi = DT_INST_PROP(n, res_x_cpi),                                                   \
        .res_y_cpi = DT_INST_PROP(n, res_y_cpi),                                                   \
        .motion_read_interval_ms = DT_INST_PROP(n, motion_read_interval_ms),                       \
        .invert_x = DT_INST_PROP(n, invert_x),                                                     \
        .invert_y = DT_INST_PROP(n, invert_y),                                                     \
        .swap_xy = DT_INST_PROP(n, swap_xy),                                                       \
        .sleep1_enable = DT_INST_PROP(n, sleep1_enable),                                           \
        .sleep2_enable = DT_INST_PROP(n, sleep2_enable),                                           \
    };                                                                                             \
                                                                                                   \
    static struct pat9125_data pat9125_data_##n;                                                   \
                                                                                                   \
    PM_DEVICE_DT_INST_DEFINE(n, pat9125_pm_action);                                                \
                                                                                                   \
    DEVICE_DT_INST_DEFINE(n, pat9125_init, PM_DEVICE_DT_INST_GET(n), &pat9125_data_##n,            \
                          &pat9125_cfg_##n, POST_KERNEL, CONFIG_INPUT_INIT_PRIORITY, NULL);

DT_INST_FOREACH_STATUS_OKAY(PAT9125_INIT)
