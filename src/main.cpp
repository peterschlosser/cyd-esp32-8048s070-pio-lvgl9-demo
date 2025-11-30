#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>
#include <driver/i2c.h>
#include <lvgl.h>
#include <demos/lv_demos.h>

static const char *TAG = "main"; // logging

class LGFX : public lgfx::LGFX_Device
{
public:
  lgfx::Bus_RGB _bus_instance;
  lgfx::Panel_RGB _panel_instance;
  lgfx::Light_PWM _light_instance;
  lgfx::Touch_GT911 _touch_instance;

  LGFX(void)
  {
    {
      auto cfg = _panel_instance.config();

      cfg.memory_width = 800;
      cfg.memory_height = 480;
      cfg.panel_width = 800;
      cfg.panel_height = 480;

      cfg.offset_x = 0;
      cfg.offset_y = 0;

      _panel_instance.config(cfg);
    }

    {
      auto cfg = _panel_instance.config_detail();

      cfg.use_psram = 1;

      _panel_instance.config_detail(cfg);
    }

    {
      auto cfg = _bus_instance.config();
      cfg.panel = &_panel_instance;
      cfg.pin_d0 = GPIO_NUM_15;  // B0
      cfg.pin_d1 = GPIO_NUM_7;   // B1
      cfg.pin_d2 = GPIO_NUM_6;   // B2
      cfg.pin_d3 = GPIO_NUM_5;   // B3
      cfg.pin_d4 = GPIO_NUM_4;   // B4
      cfg.pin_d5 = GPIO_NUM_9;   // G0
      cfg.pin_d6 = GPIO_NUM_46;  // G1
      cfg.pin_d7 = GPIO_NUM_3;   // G2
      cfg.pin_d8 = GPIO_NUM_8;   // G3
      cfg.pin_d9 = GPIO_NUM_16;  // G4
      cfg.pin_d10 = GPIO_NUM_1;  // G5
      cfg.pin_d11 = GPIO_NUM_14; // R0
      cfg.pin_d12 = GPIO_NUM_21; // R1
      cfg.pin_d13 = GPIO_NUM_47; // R2
      cfg.pin_d14 = GPIO_NUM_48; // R3
      cfg.pin_d15 = GPIO_NUM_45; // R4

      cfg.pin_henable = GPIO_NUM_41;
      cfg.pin_vsync = GPIO_NUM_40;
      cfg.pin_hsync = GPIO_NUM_39;
      cfg.pin_pclk = GPIO_NUM_42;
      cfg.freq_write = 12000000; // reduce jitter

      cfg.hsync_polarity = 0;
      cfg.hsync_front_porch = 80;
      cfg.hsync_pulse_width = 4;
      cfg.hsync_back_porch = 16;
      cfg.vsync_polarity = 0;
      cfg.vsync_front_porch = 22;
      cfg.vsync_pulse_width = 4;
      cfg.vsync_back_porch = 4;
      cfg.pclk_idle_high = 1;
      _bus_instance.config(cfg);
    }
    _panel_instance.setBus(&_bus_instance);

    {
      auto cfg = _light_instance.config();
      cfg.pin_bl = GPIO_NUM_2;
      _light_instance.config(cfg);
    }
    _panel_instance.light(&_light_instance);

    {
      auto cfg = _touch_instance.config();
      cfg.x_min = 0;
      cfg.x_max = 800;
      cfg.y_min = 0;
      cfg.y_max = 480;
      cfg.pin_int = GPIO_NUM_NC;
      cfg.bus_shared = true;
      cfg.offset_rotation = 0;
      cfg.i2c_port = I2C_NUM_1;
      cfg.pin_sda = GPIO_NUM_19;
      cfg.pin_scl = GPIO_NUM_20;
      cfg.pin_rst = GPIO_NUM_38;
      cfg.freq = 400000;
      cfg.i2c_addr = 0x14; // 0x5D , 0x14
      _touch_instance.config(cfg);
      _panel_instance.setTouch(&_touch_instance);
    }

    setPanel(&_panel_instance);
  }
};

LGFX gfx;

#define _LV_DISP_DRAW_BUF_SIZE (gfx.width() * gfx.height() * sizeof(lv_color_t) / 8)
lv_color_t *lv_buffer[2];
lv_display_t *lv_display;
lv_indev_t *lv_input;

void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
  uint16_t *buf16 = (uint16_t *)px_map;
  if (gfx.getStartCount() == 0)
  {
    gfx.startWrite();
  }
  gfx.pushImageDMA(area->x1,
                   area->y1,
                   area->x2 - area->x1 + 1,
                   area->y2 - area->y1 + 1,
                   buf16);
  gfx.endWrite();
  lv_disp_flush_ready(disp);
}

void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data)
{
  uint16_t touchX, touchY;
  const uint32_t touchDebounce = 10; // ms
  static uint32_t lastTouchTime = 0;
  if (millis() - lastTouchTime < touchDebounce)
  {
    return;
  }

  bool touched = gfx.getTouch(&touchX, &touchY);
  data->state = LV_INDEV_STATE_RELEASED;

  if (touched)
  {
    lastTouchTime = millis();
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = touchX;
    data->point.y = touchY;
  }
}

uint32_t my_tick(void)
{
  return millis();
}

void setup()
{
  Serial.begin(115200);
  while (!Serial)
  {
    delay(100);
  }
  ESP_LOGI(TAG, "setup started.");

  ESP_LOGI(TAG, "starting LovyanGFX %d.%d.%d display...", LGFX_VERSION_MAJOR, LGFX_VERSION_MINOR, LGFX_VERSION_PATCH);
  gfx.begin();
  gfx.initDMA();
  gfx.setBrightness(128);           // adjust as desired (1-255)
  gfx.setCursor(0, 0);              // optional
  gfx.setTextColor(0xFFFF, 0x0000); // optional
  gfx.setTextSize(1);               // optional
  gfx.setTextWrap(false);           // optional
  ESP_LOGI(TAG, "display started.");

  ESP_LOGI(TAG, "starting LvGL %d.%d.%d gui...", LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH);
  lv_init();
  lv_buffer[0] = (lv_color_t *)heap_caps_aligned_alloc(64, _LV_DISP_DRAW_BUF_SIZE, /* MALLOC_CAP_DMA | */ MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  lv_buffer[1] = (lv_color_t *)heap_caps_aligned_alloc(64, _LV_DISP_DRAW_BUF_SIZE, /* MALLOC_CAP_DMA | */ MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (lv_buffer[0] == nullptr || lv_buffer[1] == nullptr)
  {
    ESP_LOGI(TAG, "buffer allocation failure, aborting.");
    while (1)
      yield();
  }
  lv_display = lv_display_create(gfx.width(), gfx.height());
  lv_display_set_color_format(lv_display, LV_COLOR_FORMAT_RGB565_SWAPPED);
  lv_display_set_flush_cb(lv_display, my_disp_flush);
  lv_display_set_buffers(lv_display, lv_buffer[0], lv_buffer[1], _LV_DISP_DRAW_BUF_SIZE, LV_DISPLAY_RENDER_MODE_PARTIAL);

  lv_input = lv_indev_create();
  lv_indev_set_type(lv_input, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(lv_input, my_touchpad_read);

  lv_tick_set_cb(my_tick);

  lv_demo_widgets();
  ESP_LOGI(TAG, "gui started.");
}

void loop()
{
  lv_timer_handler();

  delay(1);
}
