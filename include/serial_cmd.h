#ifndef SERIAL_CMD_H
#define SERIAL_CMD_H

using RunStartCallback = void(*)(const char* description);
using RunStopCallback  = void(*)();

void serial_cmd_set_run_callbacks(RunStartCallback start_cb, RunStopCallback stop_cb);
void serial_init();
void serial_loop();
void run_mgmt_init();
void run_mgmt_loop();
void print_status();

#endif // SERIAL_CMD_H
