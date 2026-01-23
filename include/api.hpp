#ifndef KEYBOARD_API_HPP
#define KEYBOARD_API_HPP

#include <civetweb.h>

int presets_api_handler(struct mg_connection *conn, void *cbdata);
int input_release_handler(struct mg_connection *conn, void *cbdata);
int config_api_handler(struct mg_connection *conn, void *cbdata);
int oscillator_api_handler(struct mg_connection *conn, void *cbdata);
int waveform_api_handler(struct mg_connection *conn, void *cbdata);
int waveform_combined_api_handler(struct mg_connection *conn, void *cbdata);
int input_push_handler(struct mg_connection *conn, void *cbdata);
int recorder_handler(struct mg_connection *conn, void *cbdata);

#endif
