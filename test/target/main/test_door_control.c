#include "door_control.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"

#if CONFIG_DOOR_LOCK_ACTIVE_HIGH
#define UNLOCKED_LEVEL 1
#else
#define UNLOCKED_LEVEL 0
#endif

static void init_once(void) {
  static bool is_initialised;
  if (!is_initialised) {
    TEST_ASSERT_EQUAL(ESP_OK, door_control_init());
    is_initialised = true;
  }
  TEST_ASSERT_EQUAL(ESP_OK, door_control_close());
}

static int lock_level(void) {
  return gpio_get_level((gpio_num_t)CONFIG_DOOR_LOCK_GPIO);
}

TEST_CASE("door starts locked and opens", "[door]") {
  init_once();
  TEST_ASSERT_FALSE(door_control_is_open());
  TEST_ASSERT_EQUAL_INT(1 - UNLOCKED_LEVEL, lock_level());

  TEST_ASSERT_EQUAL(ESP_OK, door_control_open(1000));
  TEST_ASSERT_TRUE(door_control_is_open());
  TEST_ASSERT_EQUAL_INT(UNLOCKED_LEVEL, lock_level());

  TEST_ASSERT_EQUAL(ESP_OK, door_control_close());
  TEST_ASSERT_FALSE(door_control_is_open());
  TEST_ASSERT_EQUAL_INT(1 - UNLOCKED_LEVEL, lock_level());
}

TEST_CASE("door locks by itself after hold time", "[door]") {
  init_once();
  TEST_ASSERT_EQUAL(ESP_OK, door_control_open(50));
  vTaskDelay(pdMS_TO_TICKS(20));
  TEST_ASSERT_TRUE(door_control_is_open());
  vTaskDelay(pdMS_TO_TICKS(60));
  TEST_ASSERT_FALSE(door_control_is_open());
  TEST_ASSERT_EQUAL_INT(1 - UNLOCKED_LEVEL, lock_level());
}

TEST_CASE("opening again restarts the hold time", "[door]") {
  init_once();
  TEST_ASSERT_EQUAL(ESP_OK, door_control_open(60));
  vTaskDelay(pdMS_TO_TICKS(40));
  TEST_ASSERT_EQUAL(ESP_OK, door_control_open(60));
  vTaskDelay(pdMS_TO_TICKS(40));
  TEST_ASSERT_TRUE(door_control_is_open());
  vTaskDelay(pdMS_TO_TICKS(40));
  TEST_ASSERT_FALSE(door_control_is_open());
}

TEST_CASE("open with zero hold is refused", "[door]") {
  init_once();
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, door_control_open(0));
  TEST_ASSERT_FALSE(door_control_is_open());
}
