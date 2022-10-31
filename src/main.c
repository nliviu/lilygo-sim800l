/*
 * Copyright (c) 2020-2022 Liviu Nicolescu <nliviu@gmail.com>
 * All rights reserved
 *
 * Licensed under the Apache License, Version 2.0 (the ""License"");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an ""AS IS"" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "mgos_pppos.h"

#include "mgos.h"

/* clang-format off */
const char *cloud_type_stringify(enum mgos_cloud_type type) {
  const char *s = "N/A";
  switch (type) {
    case MGOS_CLOUD_MQTT:   s = "MGOS_CLOUD_MQTT";   break;
    case MGOS_CLOUD_DASH:   s = "MGOS_CLOUD_DASH";   break;
    case MGOS_CLOUD_AWS:    s = "MGOS_CLOUD_AWS";    break;
    case MGOS_CLOUD_AZURE:  s = "MGOS_CLOUD_AZURE";  break;
    case MGOS_CLOUD_GCP:    s = "MGOS_CLOUD_GCP";    break;
    case MGOS_CLOUD_WATSON: s = "MGOS_CLOUD_WATSON"; break;
    default: break;
  }
  return s;
}
/* clang-format on */

static void timer_cb(void *arg) {
  time_t t = time(0);
  struct tm timeinfo;
  localtime_r(&t, &timeinfo);
  char timestamp[24];
  strftime(timestamp, sizeof(timestamp), "%F %T", &timeinfo);
  LOG(LL_INFO, ("localtime: %s", timestamp));
  (void) arg;
}

static void modem_power_on_timer_cb(void *arg) {
  int power_on = (int) arg;
  mgos_gpio_setup_output(power_on, true);

  mgos_pppos_connect(0);
}

static void modem_start(void) {
  int power_ctrl = mgos_sys_config_get_modem_power_ctrl();
  if (power_ctrl > 0) {
    mgos_gpio_setup_output(power_ctrl, true);
    LOG(LL_INFO, ("%s - set power_ctrl on", __FUNCTION__));
    int power_on = mgos_sys_config_get_modem_power_on_pin();
    mgos_gpio_setup_output(power_on, false);
    mgos_set_timer(mgos_sys_config_get_modem_power_on_interval(), 0,
                   modem_power_on_timer_cb, (void *) power_on);
  }
}

static void net_event_handler(int ev, void *evd, void *arg) {
  const struct mgos_net_event_data *data =
      (const struct mgos_net_event_data *) evd;
  struct app_ctx *app_ctx = (struct app_ctx *) arg;
  const char *msg = "";
  switch (ev) {
    case MGOS_NET_EV_DISCONNECTED:
      msg = "MGOS_NET_EV_DISCONNECTED";
      break;
    case MGOS_NET_EV_CONNECTING:
      msg = "MGOS_NET_EV_CONNECTING";
      break;
    case MGOS_NET_EV_CONNECTED:
      msg = "MGOS_NET_EV_CONNECTED";
      // modem_run_cmds();
      break;
    case MGOS_NET_EV_IP_ACQUIRED: {
      msg = "MGOS_NET_EV_IP_ACQUIRED";
      struct mgos_net_ip_info ip_info;
      mgos_net_get_ip_info(data->if_type, data->if_instance, &ip_info);
      char ip[16], gw[16], dns[16];
      memset(ip, 0, sizeof(ip));
      memset(gw, 0, sizeof(gw));
      memset(dns, 0, sizeof(dns));
      mgos_net_ip_to_str(&ip_info.ip, ip);
      mgos_net_ip_to_str(&ip_info.gw, gw);
      mgos_net_ip_to_str(&ip_info.dns, dns);
      LOG(LL_INFO, ("Got IP - ip=%s, gw=%s, dns=%s", ip, gw, dns));
      break;
    }
    default: {
      static char buf[16];
      snprintf(buf, sizeof(buf), "ev=%d", ev);
      msg = buf;
    }
  }
  LOG(LL_INFO, ("%s", msg));
}

static void cloud_cb(int ev, void *evd, void *arg) {
  struct mgos_cloud_arg *ca = (struct mgos_cloud_arg *) evd;
  switch (ev) {
    case MGOS_EVENT_CLOUD_CONNECTED: {
      LOG(LL_INFO, ("%s - Cloud connected (%s)", __FUNCTION__,
                    cloud_type_stringify(ca->type)));
      break;
    }
    case MGOS_EVENT_CLOUD_DISCONNECTED: {
      LOG(LL_INFO, ("%s - Cloud disconnected (%s)", __FUNCTION__,
                    cloud_type_stringify(ca->type)));
      break;
    }
  }
}

enum mgos_app_init_result mgos_app_init(void) {
  modem_start();

  mgos_event_add_group_handler(MGOS_EVENT_GRP_NET, net_event_handler, NULL);
  mgos_event_add_handler(MGOS_EVENT_CLOUD_CONNECTED, cloud_cb, NULL);
  mgos_event_add_handler(MGOS_EVENT_CLOUD_DISCONNECTED, cloud_cb, NULL);

  mgos_set_timer(10000 /* ms */, MGOS_TIMER_REPEAT, timer_cb, NULL);
  return MGOS_APP_INIT_SUCCESS;
}
