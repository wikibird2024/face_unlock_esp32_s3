#include "console_commands.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "door_control.h"
#include "esp_check.h"
#include "esp_console.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "system_logic.h"

static const char *TAG = "console";

static face_db_t *s_face_db;

static bool face_db_ready(void) {
  if (s_face_db == NULL) {
    printf("Face store not available (see boot log).\n");
    return false;
  }
  return true;
}

static int report(esp_err_t err) {
  if (err != ESP_OK) {
    printf("Failed: %s\n", esp_err_to_name(err));
    return 1;
  }
  return 0;
}

static int command_status(int argc, char **argv) {
  (void)argc;
  (void)argv;
  system_logic_status_t status;
  if (system_logic_get_status(&status) == ESP_OK) {
    printf("state            %s\n", unlock_state_name(status.state));
    printf("frames           %lu (with face %lu)\n",
           (unsigned long)status.frame_count,
           (unsigned long)status.face_frame_count);
    printf("inference        %lu ms\n",
           (unsigned long)status.last_inference_ms);
    printf("last best match  id %u, similarity %.3f (need %.2f)\n",
           status.last_face_id, status.last_similarity, status.min_similarity);
    if (status.waiting_request_id != 0) {
      printf("approval         waiting #%lu (approve / deny %lu)\n",
             (unsigned long)status.waiting_request_id,
             (unsigned long)status.waiting_request_id);
    }
  }
  printf("door             %s\n", door_control_is_open() ? "open" : "locked");
  if (s_face_db != NULL) {
    printf("faces            %u / %u (skipped at boot: %u)\n",
           (unsigned)face_db_count(s_face_db),
           (unsigned)face_db_capacity(s_face_db),
           (unsigned)face_db_skipped_count(s_face_db));
  }
  printf("free heap        internal %u, psram %u\n",
         (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
         (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
  return 0;
}

static int command_list(int argc, char **argv) {
  (void)argc;
  (void)argv;
  if (!face_db_ready()) {
    return 1;
  }
  const size_t count = face_db_count(s_face_db);
  if (count == 0) {
    printf("No faces enrolled. Use: enroll <name>\n");
    return 0;
  }
  printf("  id  name\n");
  for (size_t i = 0; i < count; i++) {
    face_db_entry_t entry;
    if (face_db_get_entry(s_face_db, i, &entry) == FACE_DB_OK) {
      printf("%4u  %s\n", entry.id, entry.name);
    }
  }
  return 0;
}

static int command_enroll(int argc, char **argv) {
  if (argc != 2) {
    printf("Usage: enroll <name>\n");
    return 1;
  }
  if (!face_db_ready()) {
    return 1;
  }
  if (strlen(argv[1]) > FACE_DB_NAME_MAX_LEN) {
    printf("Name too long (max %u characters)\n", FACE_DB_NAME_MAX_LEN);
    return 1;
  }
  int result = report(system_logic_request_enroll(argv[1]));
  if (result == 0) {
    printf("Look at the camera. Watch the log for the result.\n");
  }
  return result;
}

static int command_cancel(int argc, char **argv) {
  (void)argc;
  (void)argv;
  return report(system_logic_request_cancel_enroll());
}

static int command_delete(int argc, char **argv) {
  if (argc != 2) {
    printf("Usage: delete <id>   (ids from 'list')\n");
    return 1;
  }
  if (!face_db_ready()) {
    return 1;
  }
  char *end = NULL;
  const unsigned long id = strtoul(argv[1], &end, 10);
  if (end == argv[1] || *end != '\0' || id == 0 || id > UINT16_MAX) {
    printf("Not a valid id: %s\n", argv[1]);
    return 1;
  }
  face_db_status_t status = face_db_delete(s_face_db, (uint16_t)id);
  printf("%s\n",
         status == FACE_DB_OK ? "Deleted" : face_db_status_name(status));
  return status == FACE_DB_OK ? 0 : 1;
}

static int command_clear(int argc, char **argv) {
  if (argc != 2 || strcmp(argv[1], "yes") != 0) {
    printf("This deletes every enrolled face. Type: clear yes\n");
    return 1;
  }
  if (!face_db_ready()) {
    return 1;
  }
  face_db_status_t status = face_db_clear(s_face_db);
  printf("%s\n", status == FACE_DB_OK ? "All faces deleted"
                                      : face_db_status_name(status));
  return status == FACE_DB_OK ? 0 : 1;
}

static int command_unlock(int argc, char **argv) {
  (void)argc;
  (void)argv;
  return report(system_logic_request_unlock());
}

/* approve <id> / deny <id>: the console stands in for the phone. */
static int command_answer(int argc, char **argv, bool is_approve) {
  if (argc != 2) {
    printf("Usage: %s <id>   (id from the log or 'status')\n", argv[0]);
    return 1;
  }
  char *end = NULL;
  errno = 0;
  const unsigned long request_id = strtoul(argv[1], &end, 10);
  if (end == argv[1] || *end != '\0' || errno == ERANGE || request_id == 0) {
    printf("Not a valid id: %s\n", argv[1]);
    return 1;
  }
  return report(is_approve ? system_logic_request_approve((uint32_t)request_id)
                           : system_logic_request_deny((uint32_t)request_id));
}

static int command_approve(int argc, char **argv) {
  return command_answer(argc, argv, true);
}

static int command_deny(int argc, char **argv) {
  return command_answer(argc, argv, false);
}

static esp_err_t register_commands(void) {
  const esp_console_cmd_t commands[] = {
      {.command = "status",
       .help = "Show state, last match, faces and memory",
       .func = command_status},
      {.command = "list", .help = "List enrolled faces", .func = command_list},
      {.command = "enroll",
       .help = "Enroll the next face seen: enroll <name>",
       .hint = "<name>",
       .func = command_enroll},
      {.command = "cancel",
       .help = "Cancel a running enroll",
       .func = command_cancel},
      {.command = "delete",
       .help = "Delete one face: delete <id>",
       .hint = "<id>",
       .func = command_delete},
      {.command = "clear",
       .help = "Delete all faces: clear yes",
       .hint = "yes",
       .func = command_clear},
      {.command = "unlock",
       .help = "Open the door now (manual override)",
       .func = command_unlock},
      {.command = "approve",
       .help = "Approve a face waiting for approval: approve <id>",
       .hint = "<id>",
       .func = command_approve},
      {.command = "deny",
       .help = "Deny a face waiting for approval: deny <id>",
       .hint = "<id>",
       .func = command_deny},
  };
  for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
    ESP_RETURN_ON_ERROR(esp_console_cmd_register(&commands[i]), TAG,
                        "register '%s' failed", commands[i].command);
  }
  return ESP_OK;
}

esp_err_t console_commands_start(face_db_t *face_db) {
  s_face_db = face_db;

  esp_console_repl_t *repl = NULL;
  esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
  repl_config.prompt = "face>";
  repl_config.max_cmdline_length = 128;

#if CONFIG_ESP_CONSOLE_UART_DEFAULT || CONFIG_ESP_CONSOLE_UART_CUSTOM
  esp_console_dev_uart_config_t device_config =
      ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
  ESP_RETURN_ON_ERROR(
      esp_console_new_repl_uart(&device_config, &repl_config, &repl), TAG,
      "uart repl failed");
#elif CONFIG_ESP_CONSOLE_USB_CDC
  esp_console_dev_usb_cdc_config_t device_config =
      ESP_CONSOLE_DEV_CDC_CONFIG_DEFAULT();
  ESP_RETURN_ON_ERROR(
      esp_console_new_repl_usb_cdc(&device_config, &repl_config, &repl), TAG,
      "usb cdc repl failed");
#elif CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
  esp_console_dev_usb_serial_jtag_config_t device_config =
      ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
  ESP_RETURN_ON_ERROR(
      esp_console_new_repl_usb_serial_jtag(&device_config, &repl_config, &repl),
      TAG, "usb serial jtag repl failed");
#else
#error                                                                         \
    "No console output selected in menuconfig (Component config > ESP System Settings > Channel for console output)"
#endif

  /* esp_console_new_repl_*() already registered "help". */
  ESP_RETURN_ON_ERROR(register_commands(), TAG, "commands failed");
  ESP_RETURN_ON_ERROR(esp_console_start_repl(repl), TAG, "repl start failed");
  return ESP_OK;
}
