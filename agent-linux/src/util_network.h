#ifndef UTIL_NETWORK_H
#define UTIL_NETWORK_H

void cps_connect_to_all_servers();
int cps_send_data(server_info_t *si, char *str, size_t len);

#endif
/* vim: set filetype=c fdm=marker sw=4 ts=4 et : */

