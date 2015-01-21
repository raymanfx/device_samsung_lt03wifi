/*
 * Copyright (C) 2014, The CyanogenMod Project <http://www.cyanogenmod.org>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define LOG_TAG "Lt03wifi PowerHAL"
#include <utils/Log.h>

#include <hardware/hardware.h>
#include <hardware/power.h>

#define BOOSTPULSE_PATH "/sys/devices/system/cpu/cpufreq/interactive/boostpulse"

struct lt03wifi_power_module {
    struct power_module base;
    pthread_mutex_t lock;
    int boostpulse_fd;
    int boostpulse_warned;
    const char *touchscreen_power_path;
};

static void sysfs_write(const char *path, char *s) {
    char buf[80];
    int len;
    int fd = open(path, O_WRONLY);

    if (fd < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error opening %s: %s\n", path, buf);
        return;
    }

    len = write(fd, s, strlen(s));
    if (len < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error writing to %s: %s\n", path, buf);
    }

    close(fd);
}

static void init_touchscreen_power_path(struct lt03wifi_power_module *lt03wifi)
{
    char buf[80];
    const char dir[] = "/sys/class/input/input1/enabled";
    const char filename[] = "enabled";
    DIR *d;
    struct dirent *de;
    char *path;
    int pathsize;

    d = opendir(dir);
    if (d == NULL) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error opening directory %s: %s\n", dir, buf);
        return;
    }
    while ((de = readdir(d)) != NULL) {
        if (strncmp("input", de->d_name, 5) == 0) {
            pathsize = strlen(dir) + strlen(de->d_name) + sizeof(filename) + 2;
            path = malloc(pathsize);
            if (path == NULL) {
                strerror_r(errno, buf, sizeof(buf));
                ALOGE("Out of memory: %s\n", buf);
                return;
            }
            snprintf(path, pathsize, "%s/%s/%s", dir, de->d_name, filename);
            lt03wifi->touchscreen_power_path = path;
            goto done;
        }
    }
    ALOGE("Error failed to find input dir in %s\n", dir);
done:
    closedir(d);
}

static void power_init(struct power_module *module)
{
    struct lt03wifi_power_module *lt03wifi = (struct lt03wifi_power_module *) module;
    struct dirent **namelist;
    int n;

    sysfs_write("/sys/devices/system/cpu/cpufreq/interactive/timer_rate",
                "20000");
    sysfs_write("/sys/devices/system/cpu/cpufreq/interactive/timer_slack",
                "20000");
    sysfs_write("/sys/devices/system/cpu/cpufreq/interactive/min_sample_time",
                "40000");
    sysfs_write("/sys/devices/system/cpu/cpufreq/interactive/hispeed_freq",
                "1000000");
    sysfs_write("/sys/devices/system/cpu/cpufreq/interactive/go_hispeed_load",
                "99");
    sysfs_write("/sys/devices/system/cpu/cpufreq/interactive/target_loads", "70 1200000:70 1300000:75 1400000:80 1500000:99");
    sysfs_write("/sys/devices/system/cpu/cpufreq/interactive/above_hispeed_delay",
                "80000");
    sysfs_write("/sys/devices/system/cpu/cpufreq/interactive/boostpulse_duration",
                "500000");
    
    init_touchscreen_power_path(lt03wifi);
}

static void power_set_interactive(struct power_module *module, int on)
{
    struct lt03wifi_power_module *lt03wifi = (struct lt03wifi_power_module *) module;
    
    ALOGV("power_set_interactive: %d\n", on);
    
    sysfs_write(lt03wifi->touchscreen_power_path, on ? "1" : "0");
    
    ALOGV("power_set_interactive: %d done\n", on);
}

static int boostpulse_open(struct lt03wifi_power_module *lt03wifi)
{
    char buf[80];

    pthread_mutex_lock(&lt03wifi->lock);

    if (lt03wifi->boostpulse_fd < 0) {
        lt03wifi->boostpulse_fd = open(BOOSTPULSE_PATH, O_WRONLY);

        if (lt03wifi->boostpulse_fd < 0) {
            if (!lt03wifi->boostpulse_warned) {
                strerror_r(errno, buf, sizeof(buf));
                ALOGE("Error opening %s: %s\n", BOOSTPULSE_PATH, buf);
                lt03wifi->boostpulse_warned = 1;
            }
        }
    }

    pthread_mutex_unlock(&lt03wifi->lock);
    return lt03wifi->boostpulse_fd;
}

static void lt03wifi_power_hint(struct power_module *module, power_hint_t hint,
                       void *data) {
    struct lt03wifi_power_module *lt03wifi = (struct lt03wifi_power_module *) module;
    char buf[80];
    int len;
    switch (hint) {
    case POWER_HINT_INTERACTION:
        if (boostpulse_open(lt03wifi) >= 0) {
            len = write(lt03wifi->boostpulse_fd, "1", 1);

            if (len < 0) {
                strerror_r(errno, buf, sizeof(buf));
                ALOGE("Error writing to %s: %s\n", BOOSTPULSE_PATH, buf);
            }
        }

        break;

    default:
        break;
    }
}

static struct hw_module_methods_t power_module_methods = {
    .open = NULL,
};

struct lt03wifi_power_module HAL_MODULE_INFO_SYM = {
    base: {
        common: {
            tag: HARDWARE_MODULE_TAG,
            module_api_version: POWER_MODULE_API_VERSION_0_2,
            hal_api_version: HARDWARE_HAL_API_VERSION,
            id: POWER_HARDWARE_MODULE_ID,
            name: "Lt03wifi Power HAL",
            author: "The CyanogenMod Project",
            methods: &power_module_methods,
        },

        init: power_init,
        setInteractive: power_set_interactive,
        powerHint: lt03wifi_power_hint,
    },
    
    lock: PTHREAD_MUTEX_INITIALIZER,
    boostpulse_fd: -1,
    boostpulse_warned: 0,
};
