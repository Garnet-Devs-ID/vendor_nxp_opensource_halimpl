/******************************************************************************
 *  Copyright (c) 2016-18, The Linux Foundation. All rights reserved.
 *  Not a Contribution.
 *
 *  Copyright (C) 2011-2012 Broadcom Corporation
 *
 *  The original Work has been changed by NXP.
 *
 *  Copyright 2013-2021 NXP
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/
/******************************************************************************
 *  Changes from Qualcomm Innovation Center are provided under the following license:
 *
 *  Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 *  SPDX-License-Identifier: BSD-3-Clause-Clear
 ******************************************************************************/
 /**
  * @file phNxpConfig.cpp
  * @date 24 Aug 2016
  * @brief File containing code for dynamic selection of config files based on target.
  *
  * The target device has to be configured with some primary setting while booting.So a
  * config file will be picked while the target is booted. Here based on the target device
  * a configuration file will be selected dynamically and the device will be configured.
  */

#include <stdio.h>
#include <sys/stat.h>
#include <list>
#include <string>
#include <vector>
#include <log/log.h>
#include <android-base/properties.h>

#include <phNxpConfig.h>
#include <phNxpLog.h>
#include <log/log.h>
#include <cutils/properties.h>
#include <errno.h>
#include "sparse_crc32.h"
#include "phNqChipInfo.h"

#if GENERIC_TARGET
const char alternative_config_path[] = "/data/vendor/nfc/";
#else
const char alternative_config_path[] = "";
#endif

#if 1
const char* transport_config_paths[] = {"/odm/etc/", "/vendor/etc/", "/etc/"};
#else
const char* transport_config_paths[] = {"res/"};
#endif
const int transport_config_path_size =
    (sizeof(transport_config_paths) / sizeof(transport_config_paths[0]));

#define config_name "libnfc-nxp.conf"
#define extra_config_base "libnfc-"
#define extra_config_ext ".conf"
#define IsStringValue 0x80000000

typedef enum {
  CONF_FILE_NXP = 0x00,
  CONF_FILE_NXP_RF,
  CONF_FILE_NXP_TRANSIT
}tNXP_CONF_FILE;

const char rf_config_timestamp_path[] =
        "/data/vendor/nfc/libnfc-nxpRFConfigState.bin";
const char tr_config_timestamp_path[] =
    "/data/vendor/nfc/libnfc-nxpTransitConfigState.bin";
const char config_timestamp_path[] =
        "/data/vendor/nfc/libnfc-nxpConfigState.bin";
char nxp_rf_config_path[256] =
        "/system/vendor/libnfc-nxp_RF.conf";
#if (defined(__arm64__) || defined(__aarch64__) || defined(_M_ARM64))
char Fw_Lib_Path[256] =
        "/vendor/lib64/libsn100u_fw.so";
#else
char Fw_Lib_Path[256] =
        "/vendor/lib/libsn100u_fw.so";
#endif
const char transit_config_path[] = "/data/vendor/nfc/libnfc-nxpTransit.conf";

extern char default_nxp_config_path[];

/**
 *  @brief target platform ID values.
 */

typedef enum
{
  CONFIG_GENERIC                         = 0x00,
  MTP_TYPE_DEFAULT                       = 0x01, /**< default MTP config. DC DC ON */
  QRD_TYPE_DEFAULT                       = 0x02, /**< default QRD config DC DC OFF */
  MTP_TYPE_1                             = 0x03, /**< mtp config type1 : newer chip */
  MTP_TYPE_2                             = 0x04, /**< mtp config type2 TBD */
  QRD_TYPE_1                             = 0x05, /**< qrd config type1 DC DC ON*/
  QRD_TYPE_2                             = 0x06, /**< qrd config type2  Newer chip */
  MTP_TYPE_NQ3XX                         = 0x07, /**< mtp config : for NQ3XX chip */
  QRD_TYPE_NQ3XX                         = 0x08, /**< qrd config : for NQ3XX chip */
  MTP_TYPE_NQ4XX                         = 0x09, /**< mtp config : for NQ4XX chip */
  QRD_TYPE_NQ4XX                         = 0x10, /**< qrd config : for NQ4XX chip */
  MTP_TYPE_SN100                         = 0x11, /**< mtp config : for SN100 chip */
  QRD_TYPE_SN100                         = 0x12, /**< qrd config : for SN100 chip */
  GENERIC_TYPE_SN220                     = 0x13, /**< generic config : for SN220 chip */
  DEFAULT_CONFIG                         = QRD_TYPE_DEFAULT, /**< default is qrd default config */
  CONFIG_INVALID                         = 0xFF
} CONFIGIDVALUE;

/**
 *  @brief Defines the soc_id values for different targets.
 */

typedef enum
{
  TARGET_GENERIC                       = 0x00,/**< new targets */
  TARGET_SM_WAIPIO                     = 457, /**< SM_WAIPIO target */
  TARGET_SM_WAIPIO_APQ                 = 482, /**< SM_WAIPIO_APQ target */
  TARGET_SM_FILLMORE                   = 506, /**< SM_FILLMORE target */
  TARGET_SM_PALIMA                     = 530, /**< SM_PALIMA target */
  TARGET_SMP_PALIMA                    = 531, /**< SMP_PALIMA target */
  TARGET_SM_NETRANI                    = 537, /**< SM_NETRANI target */
  TARGET_SMP_NETRANI                   = 583, /**< SMP_NETRANI target */
  TARGET_SM_NETRANI7                   = 613, /**< SM_NETRANI7 target */
  TARGET_SM_PALIMA_LTE_ONLY            = 540, /**< SM_PALIMA_LTE_ONLY target */
  TARGET_SM_ALAKAI                     = 552, /**< SM_ALAKAI target */
  TARGET_SM_CLARENCE                   = 568, /**< SM_CLARENCE target */
  TARGET_QCM_CLARENCE                  = 581, /**< QCM_CLARENCE IOT target */
  TARGET_QCS_CLARENCE                  = 582, /**< QCS_CLARENCE IOT target */
  TARGET_SMP_CLARENCE                  = 602, /**< SMP_CLARENCE target */
  TARGET_SM_TOFINO                     = 591, /**< SM_TOFINO target */
  TARGET_DEFAULT                       = TARGET_GENERIC, /**< new targets */
  TARGET_INVALID                       = 0xFF
} TARGETTYPE;

void readOptionalConfig(const char* optional);

size_t readConfigFile(const char* fileName, uint8_t** p_data) {
  FILE* fd = fopen(fileName, "rb");
  if (fd == nullptr) return 0;

  fseek(fd, 0L, SEEK_END);
  const size_t file_size = ftell(fd);
  rewind(fd);
  if((long)file_size < 0) {
    ALOGE("%s Invalid file size file_size = %zu\n",__func__,file_size);
    fclose(fd);
    return 0;
  }
  uint8_t* buffer = new uint8_t[file_size + 1];
  if (!buffer) {
    fclose(fd);
    return 0;
  }
  size_t read = fread(buffer, file_size, 1, fd);
  fclose(fd);

  if (read == 1) {
    buffer[file_size] = '\n';
    *p_data = buffer;
    return file_size+1;
  }
  delete[] buffer;
  return 0;
}


using namespace ::std;

bool findConfigFilePathFromTransportConfigPaths(const string& configName, string& filePath);

class CNfcParam : public string {
 public:
  CNfcParam();
  CNfcParam(const char* name, const string& value);
  CNfcParam(const char* name, unsigned long value);
  virtual ~CNfcParam();
  unsigned long numValue() const { return m_numValue; }
  const char* str_value() const { return m_str_value.c_str(); }
  size_t str_len() const { return m_str_value.length(); }

 private:
  string m_str_value;
  unsigned long m_numValue;
};

class CNfcConfig : public vector<const CNfcParam*> {
 public:
  virtual ~CNfcConfig();
  static CNfcConfig& GetInstance();
  friend void readOptionalConfig(const char* optional);
  bool isModified(tNXP_CONF_FILE aType);
  void resetModified(tNXP_CONF_FILE aType);

  bool getValue(const char* name, char* pValue, size_t len) const;
  bool getValue(const char* name, unsigned long& rValue) const;
  bool getValue(const char* name, unsigned short& rValue) const;
  bool getValue(const char* name, char* pValue, long len, long* readlen) const;
  const CNfcParam* find(const char* p_name) const;
  void readNxpTransitConfig(const char* fileName) const;
  void readNxpRFConfig(const char* fileName) const;
  void clean();

 private:
  CNfcConfig();
  bool readConfig(const char* name, bool bResetContent);
  int file_exist (const char* filename);
  int getconfiguration_id (char * config_file);
  void moveFromList();
  void moveToList();
  void add(const CNfcParam* pParam);
  void dump();
  bool isAllowed(const char* name);
  list<const CNfcParam*> m_list;
  bool mValidFile;
  bool mDynamConfig;
  uint32_t config_crc32_;
  uint32_t config_rf_crc32_;
  uint32_t config_tr_crc32_;
  string mCurrentFile;

  unsigned long state;

  inline bool Is(unsigned long f) { return (state & f) == f; }
  inline void Set(unsigned long f) { state |= f; }
  inline void Reset(unsigned long f) { state &= ~f; }
};

/**
 * @brief This function reads the hardware information from the given path.
 *
 * This function receives the path and then reads the hardware information
 * from the file present in the given path. It reads the details like whether
 * it is QRD or MTP. It reads the data from that file and stores in buffer.
 * It also receives a count which tells the number of characters to be read
 * Finally the length of the buffer is returned.
 *
 * @param path The path where the file containing hardware details to be read.
 * @param buff The hardware details that is read from that path will be stored here.
 * @param count It represents the number of characters to be read from that file.
 * @return It returns the length of the buffer.
 */

static int read_line_from_file(const char *path, char *buf, size_t count)
{
    char *fgets_ret = NULL;
    FILE *fd = NULL;
    int rv = 0;

    // opens the file to read the HW_PLATFORM detail of the target
    fd = fopen(path, "r");
    if (fd == NULL)
        return -1;

    // stores the data that is read from the given path into buf
    fgets_ret = fgets(buf, (int)count, fd);
    if (NULL != fgets_ret)
        rv = (int)strlen(buf);
    else
        rv = ferror(fd);

    fclose(fd);

    return rv;
}

/**
 * @brief This function gets the source information from the file.
 *
 * This function receives a buffer variable to store the read information
 * and also receives two different path. The hardware information may be
 * present in any one of the received path. So this function checks in
 * both the paths. This function internally uses read_line_from_file
 * function to read the check and read the hardware details in each path.
 *
 * @param buf hardware details that is read will be stored.
 * @param soc_node_path1 The first path where the file may be present.
 * @param soc_node_path2 The second path where the file may be present.
 * @return Returns the length of buffer.
 */

static int get_soc_info(char *buf, const char *soc_node_path1,
            const char *soc_node_path2)
{
    int ret = 0;

    // checks whether the hw platform detail is present in this path
    ret = read_line_from_file(soc_node_path1, buf, MAX_SOC_INFO_NAME_LEN);
    if (ret < 0) {
        // if the hw platform detail is not present in the former path it checks here
        ret = read_line_from_file(soc_node_path2, buf, MAX_SOC_INFO_NAME_LEN);
        if (ret < 0) {
            ALOGE("getting socinfo(%s, %d) failed.\n", soc_node_path1, ret);
            return ret;
        }
    }
    if (ret && buf[ret - 1] == '\n')
        buf[ret - 1] = '\0';

    return ret;
}

/**
 * @brief finds the cofiguration id value for the particular target.
 *
 * This function reads the target board platform detail and hardware
 * platform detail from the target device and generate a generic
 * config file name.If that config file is present then it will be
 * used for configuring that target. If not then based on the target
 * information a config file will be assigned.
 *
 * @param config_file The generic config file name will be stored.
 * @return it returns the config id for the target.
 */

int CNfcConfig::getconfiguration_id (char * config_file)
{
    int config_id = QRD_TYPE_DEFAULT;
    char target_type[MAX_SOC_INFO_NAME_LEN] = {'\0'};
    char soc_info[MAX_SOC_INFO_NAME_LEN] = {'\0'};
    string strPath;
    int rc = 0;
    int idx = 0;
    chip_info_t nq_chip_info;

    rc = get_soc_info(soc_info, SYSFS_SOCID_PATH1, SYSFS_SOCID_PATH2);
    if (rc < 0) {
        ALOGE("get_soc_info(SOC_ID) fail!\n");
        return DEFAULT_CONFIG;
    }
    idx = atoi(soc_info);

    rc = get_soc_info(target_type, SYSFS_HW_PLATFORM_PATH1, SYSFS_HW_PLATFORM_PATH2);
    if (rc < 0) {
        ALOGE("get_soc_info(HW_PLATFORM) fail!\n");
        return DEFAULT_CONFIG;
    }

    rc = get_chip_info(&nq_chip_info);
    if (rc) {
        ALOGE("get_chip_info() fail!\n");
    }

    // Converting the HW_PLATFORM detail that is read from target to lowercase
    for (int i=0;target_type[i];i++)
        target_type[i] = tolower(target_type[i]);

    // generating a generic config file name based on the target details
    snprintf(config_file, MAX_DATA_CONFIG_PATH_LEN, "libnfc-%s_%s.conf",
            soc_info, target_type);

    findConfigFilePathFromTransportConfigPaths(config_file, strPath);
    if (file_exist(strPath.c_str()))
        idx = 0;

    if (DEBUG)
        ALOGI("id:%d, config_file_name:%s\n", idx, config_file);

    // if target is QRD platform then config id is assigned here
    if (0 == strncmp(target_type, QRD_HW_PLATFORM, MAX_SOC_INFO_NAME_LEN)) {
        switch (idx)
        {
        case TARGET_GENERIC:
            config_id = CONFIG_GENERIC;
            break;
        case TARGET_SM_FILLMORE:
            // SN220
            config_id = GENERIC_TYPE_SN220;
            strlcpy(config_file, config_name_generic_SN220, MAX_DATA_CONFIG_PATH_LEN);
            break;
        case TARGET_SM_NETRANI:
        case TARGET_SMP_NETRANI:
        case TARGET_SM_NETRANI7:
            if (!strncmp(nq_chip_info.nq_chipid, SN100_CHIP_ID, PROPERTY_VALUE_MAX)) {
                // SN100 or SN110
                config_id = QRD_TYPE_SN100;
                strlcpy(config_file, config_name_qrd_SN100, MAX_DATA_CONFIG_PATH_LEN);
            } else {
                // SN220
                config_id = GENERIC_TYPE_SN220;
                strlcpy(config_file, config_name_generic_SN220, MAX_DATA_CONFIG_PATH_LEN);
            }
            break;
        case TARGET_SM_ALAKAI:
            if (!strncmp(nq_chip_info.nq_chipid, SN220_CHIP_ID, PROPERTY_VALUE_MAX)) {
                // SN220
                config_id = GENERIC_TYPE_SN220;
                strlcpy(config_file, config_name_generic_SN220, MAX_DATA_CONFIG_PATH_LEN);
            } else {
                // SN110 or SN100
                config_id = QRD_TYPE_SN100;
                strlcpy(config_file, config_name_qrd_SN100, MAX_DATA_CONFIG_PATH_LEN);
            }
            break;
        case TARGET_SM_WAIPIO:
        case TARGET_SM_WAIPIO_APQ:
        case TARGET_SM_PALIMA:
        case TARGET_SMP_PALIMA:
        case TARGET_SM_PALIMA_LTE_ONLY:
        case TARGET_SM_TOFINO:
        case TARGET_SM_CLARENCE:
        case TARGET_SMP_CLARENCE:
        case TARGET_QCM_CLARENCE:
        case TARGET_QCS_CLARENCE:
            // SN110 or SN100
            config_id = QRD_TYPE_SN100;
            strlcpy(config_file, config_name_qrd_SN100, MAX_DATA_CONFIG_PATH_LEN);
            break;
        default:
            config_id = QRD_TYPE_DEFAULT;
            strlcpy(config_file, config_name_qrd, MAX_DATA_CONFIG_PATH_LEN);
            break;
        }
    }
    // if target is not QRD platform then default config id is assigned here
    else {
        switch (idx)
        {
        case TARGET_GENERIC:
            config_id = CONFIG_GENERIC;
            break;
        case TARGET_SM_FILLMORE:
            // SN220
            config_id = GENERIC_TYPE_SN220;
            strlcpy(config_file, config_name_generic_SN220, MAX_DATA_CONFIG_PATH_LEN);
            break;
        case TARGET_SM_NETRANI:
        case TARGET_SMP_NETRANI:
        case TARGET_SM_NETRANI7:
           if (!strncmp(nq_chip_info.nq_chipid, SN100_CHIP_ID, PROPERTY_VALUE_MAX)) {
                // SN100 or SN110
                config_id = MTP_TYPE_SN100;
                strlcpy(config_file, config_name_mtp_SN100, MAX_DATA_CONFIG_PATH_LEN);
            } else {
                // SN220
                config_id = GENERIC_TYPE_SN220;
                strlcpy(config_file, config_name_generic_SN220, MAX_DATA_CONFIG_PATH_LEN);
            }
            break;
        case TARGET_SM_ALAKAI:
            if (!strncmp(nq_chip_info.nq_chipid, SN220_CHIP_ID, PROPERTY_VALUE_MAX)) {
                // SN220
                config_id = GENERIC_TYPE_SN220;
                strlcpy(config_file, config_name_generic_SN220, MAX_DATA_CONFIG_PATH_LEN);
            } else {
                // SN110 or SN100
                config_id = MTP_TYPE_SN100;
                strlcpy(config_file, config_name_mtp_SN100, MAX_DATA_CONFIG_PATH_LEN);
            }
            break;
        case TARGET_SM_WAIPIO:
        case TARGET_SM_WAIPIO_APQ:
        case TARGET_SM_PALIMA:
        case TARGET_SMP_PALIMA:
        case TARGET_SM_PALIMA_LTE_ONLY:
        case TARGET_SM_TOFINO:
        case TARGET_SM_CLARENCE:
        case TARGET_SMP_CLARENCE:
        case TARGET_QCM_CLARENCE:
        case TARGET_QCS_CLARENCE:
            // SN110 or SN100
            config_id = MTP_TYPE_SN100;
            strlcpy(config_file, config_name_mtp_SN100, MAX_DATA_CONFIG_PATH_LEN);
            break;
        default:
            config_id = MTP_TYPE_DEFAULT;
            strlcpy(config_file, config_name_mtp, MAX_DATA_CONFIG_PATH_LEN);
            break;
        }
    }
    if (DEBUG)
        ALOGI("platform config id:%d, config_file_name:%s\n", config_id, config_file);

    return config_id;
}

/*******************************************************************************
**
** Function:    isPrintable()
**
** Description: determine if 'c' is printable
**
** Returns:     1, if printable, otherwise 0
**
*******************************************************************************/
inline bool isPrintable(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
         (c >= '0' && c <= '9') || c == '/' || c == '_' || c == '-' || c == '.';
}

/*******************************************************************************
**
** Function:    isDigit()
**
** Description: determine if 'c' is numeral digit
**
** Returns:     true, if numerical digit
**
*******************************************************************************/
inline bool isDigit(char c, int base) {
  if ('0' <= c && c <= '9') return true;
  if (base == 16) {
    if (('A' <= c && c <= 'F') || ('a' <= c && c <= 'f')) return true;
  }
  return false;
}

/*******************************************************************************
**
** Function:    getDigitValue()
**
** Description: return numerical value of a decimal or hex char
**
** Returns:     numerical value if decimal or hex char, otherwise 0
**
*******************************************************************************/
inline int getDigitValue(char c, int base) {
  if ('0' <= c && c <= '9') return c - '0';
  if (base == 16) {
    if ('A' <= c && c <= 'F')
      return c - 'A' + 10;
    else if ('a' <= c && c <= 'f')
      return c - 'a' + 10;
  }
  return 0;
}

/*******************************************************************************
**
** Function:    findConfigFilePathFromTransportConfigPaths()
**
** Description: find a config file path with a given config name from transport
**              config paths
**
** Returns:     none
**
*******************************************************************************/
bool findConfigFilePathFromTransportConfigPaths(const string& configName,
                                                string& filePath) {
  for (int i = 0; i < transport_config_path_size - 1; i++) {
    if (configName.empty()) break;
    filePath.assign(transport_config_paths[i]);
    filePath += configName;
    struct stat file_stat;
    if (stat(filePath.c_str(), &file_stat) == 0 && S_ISREG(file_stat.st_mode)) {
      return true;
    }
  }
  filePath = "";
  return false;
}

/*******************************************************************************
**
** Function:    CNfcConfig::readConfig()
**
** Description: read Config settings and parse them into a linked list
**              move the element from linked list to a array at the end
**
** Returns:     1, if there are any config data, 0 otherwise
**
*******************************************************************************/
bool CNfcConfig::readConfig(const char* name, bool bResetContent) {
  enum {
    BEGIN_LINE = 1,
    TOKEN,
    STR_VALUE,
    NUM_VALUE,
    BEGIN_HEX,
    BEGIN_QUOTE,
    END_LINE
  };

  uint8_t* p_config = nullptr;
  size_t config_size = readConfigFile(name, &p_config);
  if (p_config == nullptr) {
    ALOGE("%s Cannot open config file %s\n", __func__, name);
    if (bResetContent) {
      ALOGE("%s Using default value for all settings\n", __func__);
      mValidFile = false;
    }
    return false;
  }

  string token;
  string strValue;
  unsigned long numValue = 0;
  CNfcParam* pParam = NULL;
  int i = 0;
  int base = 0;
  char c;
  int bflag = 0;
  state = BEGIN_LINE;

  ALOGD("readConfig; filename is %s", name);
  if(strcmp(name, nxp_rf_config_path) == 0) {
    config_rf_crc32_ = sparse_crc32(0, (const void*)p_config, (int)config_size);
  } else if (strcmp(name, transit_config_path) == 0) {
    config_tr_crc32_ = sparse_crc32(0, (const void*)p_config, (int)config_size);
  } else {
    config_crc32_ = sparse_crc32(0, (const void*)p_config, (int)config_size);
  }

  mValidFile = true;
  if (size() > 0) {
    if (bResetContent)
      clean();
    else
      moveToList();
  }

  for (size_t offset = 0; offset != config_size; ++offset) {
    c = p_config[offset];
    switch (state & 0xff) {
      case BEGIN_LINE:
        if (c == '#')
          state = END_LINE;
        else if (isPrintable(c)) {
          i = 0;
          token.erase();
          strValue.erase();
          state = TOKEN;
          token.push_back(c);
        }
        break;
      case TOKEN:
        if (c == '=') {
          token.push_back('\0');
          state = BEGIN_QUOTE;
        } else if (isPrintable(c))
          token.push_back(c);
        else
          state = END_LINE;
        break;
      case BEGIN_QUOTE:
        if (c == '"') {
          state = STR_VALUE;
          base = 0;
        } else if (c == '0')
          state = BEGIN_HEX;
        else if (isDigit(c, 10)) {
          state = NUM_VALUE;
          base = 10;
          numValue = getDigitValue(c, base);
          i = 0;
        } else if (c == '{') {
          state = NUM_VALUE;
          bflag = 1;
          base = 16;
          i = 0;
          Set(IsStringValue);
        } else
          state = END_LINE;
        break;
      case BEGIN_HEX:
        if (c == 'x' || c == 'X') {
          state = NUM_VALUE;
          base = 16;
          numValue = 0;
          i = 0;
          break;
        } else if (isDigit(c, 10)) {
          state = NUM_VALUE;
          base = 10;
          numValue = getDigitValue(c, base);
          break;
        } else if (c != '\n' && c != '\r') {
          state = END_LINE;
          break;
        }
        // fall through to numValue to handle numValue
        [[fallthrough]];
      case NUM_VALUE:
        if (isDigit(c, base)) {
          numValue *= base;
          numValue += getDigitValue(c, base);
          ++i;
        } else if (bflag == 1 &&
                   (c == ' ' || c == '\r' || c == '\n' || c == '\t')) {
          break;
        } else if (base == 16 &&
                   (c == ',' || c == ':' || c == '-' || c == ' ' || c == '}')) {
          if (c == '}') {
            bflag = 0;
          }
          if (i > 0) {
            int n = (i + 1) / 2;
            while (n-- > 0) {
              numValue = numValue >> (n * 8);
              unsigned char c = (numValue)&0xFF;
              strValue.push_back(c);
            }
          }

          Set(IsStringValue);
          numValue = 0;
          i = 0;
        } else {
          if (c == '\n' || c == '\r') {
            if (bflag == 0) {
              state = BEGIN_LINE;
            }
          } else {
            if (bflag == 0) {
              state = END_LINE;
            }
          }
          if (Is(IsStringValue) && base == 16 && i > 0) {
            int n = (i + 1) / 2;
            while (n-- > 0) strValue.push_back(((numValue >> (n * 8)) & 0xFF));
          }
          if (strValue.length() > 0)
            pParam = new CNfcParam(token.c_str(), strValue);
          else
            pParam = new CNfcParam(token.c_str(), numValue);
          add(pParam);
          strValue.erase();
          numValue = 0;
        }
        break;
      case STR_VALUE:
        if (c == '"') {
          strValue.push_back('\0');
          state = END_LINE;
          pParam = new CNfcParam(token.c_str(), strValue);
          add(pParam);
        } else if (isPrintable(c))
          strValue.push_back(c);
        break;
      case END_LINE:
        if (c == '\n' || c == '\r') state = BEGIN_LINE;
        break;
      default:
        break;
    }
  }

  delete[] p_config;

  moveFromList();
  return size() > 0;
}

/*******************************************************************************
**
** Function:    CNfcConfig::CNfcConfig()
**
** Description: class constructor
**
** Returns:     none
**
*******************************************************************************/
CNfcConfig::CNfcConfig()
    : mValidFile(true),
      mDynamConfig(true),
      config_crc32_(0),
      config_rf_crc32_(0),
      config_tr_crc32_(0),
      state(0) {}

/*******************************************************************************
**
** Function:    CNfcConfig::~CNfcConfig()
**
** Description: class destructor
**
** Returns:     none
**
*******************************************************************************/
CNfcConfig::~CNfcConfig() {}

/**
 * @brief checks whether the given file exist.
 *
 * This function gets the file name and checks whether the given file
 * exist in the particular path.Internaly it uses stat system call to
 * find the existance.
 *
 * @param filename The name of the file whose existance has to be checked.
 * @return it returns true if the given file name exist.
 */
int CNfcConfig::file_exist (const char* filename)
{
    struct stat   buffer;
    return (stat (filename, &buffer) == 0);
}

/*******************************************************************************
**
** Function:    CNfcConfig::GetInstance()
**
** Description: get class singleton object
**
** Returns:     none
**
*******************************************************************************/
CNfcConfig& CNfcConfig::GetInstance() {
  static CNfcConfig theInstance;
  int gconfigpathid=0;
  char config_name_generic[MAX_DATA_CONFIG_PATH_LEN] = {'\0'};

  if (theInstance.size() == 0 && theInstance.mValidFile) {
    string strPath;
    if (alternative_config_path[0] != '\0') {
      strPath.assign(alternative_config_path);
      strPath += config_name;
      theInstance.readConfig(strPath.c_str(), true);
      if (!theInstance.empty()) {
        return theInstance;
      }
    }
    findConfigFilePathFromTransportConfigPaths(config_name, strPath);
    //checks whether the default config file is present in th target
    if (theInstance.file_exist(strPath.c_str())) {
        ALOGI("default config file exists = %s, disables dynamic selection", strPath.c_str());
        theInstance.mDynamConfig = false;
        theInstance.readConfig(strPath.c_str(), true);
        /*
         * if libnfc-nxp.conf exists then dynamic selection will
         * be turned off by default we will not have this file.
         */
        return theInstance;
    }

    gconfigpathid = theInstance.getconfiguration_id(config_name_generic);
    findConfigFilePathFromTransportConfigPaths(config_name_generic, strPath);
    if (!(theInstance.file_exist(strPath.c_str()))) {
        ALOGI("no matching file found, using default file for stability\n");
        findConfigFilePathFromTransportConfigPaths(config_name_default, strPath);
    }
    ALOGI("config file used = %s\n",strPath.c_str());
    /*
     * overwrite default value with config file picked at runtime
     * using dynamic config feature
     */
    strlcpy(default_nxp_config_path, strPath.c_str(), MAX_DATA_CONFIG_PATH_LEN);
    theInstance.readConfig(strPath.c_str(), true);
#if (NXP_EXTNS == TRUE)
    theInstance.readNxpRFConfig(nxp_rf_config_path);
    theInstance.readNxpTransitConfig(transit_config_path);
#endif
  }
  return theInstance;
}

/*******************************************************************************
**
** Function:    CNfcConfig::getValue()
**
** Description: get a string value of a setting
**
** Returns:     true if setting exists
**              false if setting does not exist
**
*******************************************************************************/
bool CNfcConfig::getValue(const char* name, char* pValue, size_t len) const {
  const CNfcParam* pParam = find(name);
  if (pParam == NULL) return false;

  if (pParam->str_len() > 0) {
    memset(pValue, 0, len);
    memcpy(pValue, pParam->str_value(), pParam->str_len());
    return true;
  }
  return false;
}

bool CNfcConfig::getValue(const char* name, char* pValue, long len,
                          long* readlen) const {
  const CNfcParam* pParam = find(name);
  if (pParam == NULL) return false;

  if (pParam->str_len() > 0) {
    if (pParam->str_len() <= (unsigned long)len) {
      memset(pValue, 0, len);
      memcpy(pValue, pParam->str_value(), pParam->str_len());
      *readlen = pParam->str_len();
    } else {
      *readlen = -1;
    }

    return true;
  }
  return false;
}

/*******************************************************************************
**
** Function:    CNfcConfig::getValue()
**
** Description: get a long numerical value of a setting
**
** Returns:     true if setting exists
**              false if setting does not exist
**
*******************************************************************************/
bool CNfcConfig::getValue(const char* name, unsigned long& rValue) const {
  const CNfcParam* pParam = find(name);
  if (pParam == NULL) return false;

  if (pParam->str_len() == 0) {
    rValue = static_cast<unsigned long>(pParam->numValue());
    return true;
  }
  return false;
}

/*******************************************************************************
**
** Function:    CNfcConfig::getValue()
**
** Description: get a short numerical value of a setting
**
** Returns:     true if setting exists
**              false if setting does not exist
**
*******************************************************************************/
bool CNfcConfig::getValue(const char* name, unsigned short& rValue) const {
  const CNfcParam* pParam = find(name);
  if (pParam == NULL) return false;

  if (pParam->str_len() == 0) {
    rValue = static_cast<unsigned short>(pParam->numValue());
    return true;
  }
  return false;
}

/*******************************************************************************
**
** Function:    CNfcConfig::find()
**
** Description: search if a setting exist in the setting array
**
** Returns:     pointer to the setting object
**
*******************************************************************************/
const CNfcParam* CNfcConfig::find(const char* p_name) const {
  if (size() == 0) return NULL;

  for (const_iterator it = begin(), itEnd = end(); it != itEnd; ++it) {
    if (**it < p_name) {
      continue;
    } else if (**it == p_name) {
      if ((*it)->str_len() > 0) {
        NXPLOG_EXTNS_D("%s found %s=%s\n", __func__, p_name,
                       (*it)->str_value());
      } else {
        NXPLOG_EXTNS_D("%s found %s=(0x%lx)\n", __func__, p_name,
                       (*it)->numValue());
      }
      return *it;
    } else
      break;
  }
  return NULL;
}

/*******************************************************************************
**
** Function:    CNfcConfig::readNxpTransitConfig()
**
** Description: read Config settings from transit conf file
**
** Returns:     none
**
*******************************************************************************/
void CNfcConfig::readNxpTransitConfig(const char* fileName) const {
  ALOGD("readNxpTransitConfig-Enter..Reading %s", fileName);
  CNfcConfig::GetInstance().readConfig(fileName, false);
}

/*******************************************************************************
**
** Function:    CNfcConfig::readNxpRFConfig()
**
** Description: read Config settings from RF conf file
**
** Returns:     none
**
*******************************************************************************/
void CNfcConfig::readNxpRFConfig(const char* fileName) const {
  ALOGD("readNxpRFConfig-Enter..Reading %s", fileName);
  CNfcConfig::GetInstance().readConfig(fileName, false);
}

/*******************************************************************************
**
** Function:    CNfcConfig::clean()
**
** Description: reset the setting array
**
** Returns:     none
**
*******************************************************************************/
void CNfcConfig::clean() {
  if (size() == 0) return;

  for (iterator it = begin(), itEnd = end(); it != itEnd; ++it) delete *it;
  clear();
}

/*******************************************************************************
**
** Function:    CNfcConfig::Add()
**
** Description: add a setting object to the list
**
** Returns:     none
**
*******************************************************************************/
void CNfcConfig::add(const CNfcParam* pParam) {
  if (m_list.size() == 0) {
    m_list.push_back(pParam);
    return;
  }
  if ((mCurrentFile.find("nxpTransit") != std::string::npos) &&
      !isAllowed(pParam->c_str())) {
    ALOGD("%s Token restricted. Returning", __func__);
    return;
  }
  for (list<const CNfcParam*>::iterator it = m_list.begin(),
                                        itEnd = m_list.end();
       it != itEnd; ++it) {
    if (**it < pParam->c_str()) continue;
    if (**it == pParam->c_str())
      m_list.insert(m_list.erase(it), pParam);
    else
      m_list.insert(it, pParam);

    return;
  }
  m_list.push_back(pParam);
}
/*******************************************************************************
**
** Function:    CNfcConfig::dump()
**
** Description: prints all elements in the list
**
** Returns:     none
**
*******************************************************************************/
void CNfcConfig::dump() {
  ALOGD("%s Enter", __func__);

  for (list<const CNfcParam*>::iterator it = m_list.begin(),
                                        itEnd = m_list.end();
       it != itEnd; ++it) {
    if ((*it)->str_len() > 0)
      ALOGD("%s %s \t= %s", __func__, (*it)->c_str(), (*it)->str_value());
    else
      ALOGD("%s %s \t= (0x%0lX)\n", __func__, (*it)->c_str(),
            (*it)->numValue());
  }
}
/*******************************************************************************
**
** Function:    CNfcConfig::isAllowed()
**
** Description: checks if token update is allowed
**
** Returns:     true if allowed else false
**
*******************************************************************************/
bool CNfcConfig::isAllowed(const char* name) {
  string token(name);
  bool stat = false;
  if ((token.find("P2P_LISTEN_TECH_MASK") != std::string::npos) ||
      (token.find("HOST_LISTEN_TECH_MASK") != std::string::npos) ||
      (token.find("UICC_LISTEN_TECH_MASK") != std::string::npos) ||
      (token.find("NXP_ESE_LISTEN_TECH_MASK") != std::string::npos) ||
      (token.find("POLLING_TECH_MASK") != std::string::npos) ||
      (token.find("NXP_RF_CONF_BLK") != std::string::npos) ||
      (token.find("NXP_CN_TRANSIT_BLK_NUM_CHECK_ENABLE") !=
       std::string::npos) ||
      (token.find("NXP_FWD_FUNCTIONALITY_ENABLE") != std::string::npos))

  {
    stat = true;
  }
  return stat;
}
/*******************************************************************************
**
** Function:    CNfcConfig::moveFromList()
**
** Description: move the setting object from list to array
**
** Returns:     none
**
*******************************************************************************/
void CNfcConfig::moveFromList() {
  if (m_list.size() == 0) return;

  for (list<const CNfcParam *>::iterator it = m_list.begin(),
                                         itEnd = m_list.end();
       it != itEnd; ++it)
    push_back(*it);
  m_list.clear();
}

/*******************************************************************************
**
** Function:    CNfcConfig::moveToList()
**
** Description: move the setting object from array to list
**
** Returns:     none
**
*******************************************************************************/
void CNfcConfig::moveToList() {
  if (m_list.size() != 0) m_list.clear();

  for (iterator it = begin(), itEnd = end(); it != itEnd; ++it)
    m_list.push_back(*it);
  clear();
}
bool CNfcConfig::isModified(tNXP_CONF_FILE aType) {
  FILE* fd = NULL;
  bool isModified = false;

  ALOGD("isModified enter; conf file type %d", aType);
  switch (aType) {
  case CONF_FILE_NXP:
    fd = fopen(config_timestamp_path, "r+");
    break;
  case CONF_FILE_NXP_RF:
    fd = fopen(rf_config_timestamp_path, "r+");
    break;
  case CONF_FILE_NXP_TRANSIT:
    fd = fopen(tr_config_timestamp_path, "r+");
    break;
  default:
    ALOGD("Invalid conf file type");
    return false;
  }
  if (fd == nullptr) {
    ALOGE("%s Unable to open file assume modified", __func__);
    return true;
  }

  uint32_t stored_crc32 = 0;
  if (fread(&stored_crc32, sizeof(uint32_t), 1, fd) != 1) {
    ALOGE("%s File read is not successfull errno = %d", __func__, errno);
  }

  fclose(fd);
  ALOGD("stored_crc32 is %d config_crc32_ is %d", stored_crc32, config_crc32_);

  switch (aType) {
    case CONF_FILE_NXP:
      isModified = stored_crc32 != config_crc32_;
      break;
    case CONF_FILE_NXP_RF:
      isModified = stored_crc32 != config_rf_crc32_;
      break;
    case CONF_FILE_NXP_TRANSIT:
      isModified = stored_crc32 != config_tr_crc32_;
      break;
  }
  return isModified;
}

void CNfcConfig::resetModified(tNXP_CONF_FILE aType) {
  FILE* fd = NULL;

  ALOGD("resetModified enter; conf file type is %d", aType);
  switch (aType) {
  case CONF_FILE_NXP:
    fd = fopen(config_timestamp_path, "w+");
    break;
  case CONF_FILE_NXP_RF:
    fd = fopen(rf_config_timestamp_path, "w+");
    break;
  case CONF_FILE_NXP_TRANSIT:
    fd = fopen(tr_config_timestamp_path, "w+");
    break;
  default:
    ALOGD("Invalid conf file type");
    return;
  }

  if (fd == nullptr) {
    ALOGE("%s Unable to open file for writing", __func__);
    return;
  }

  switch (aType) {
    case CONF_FILE_NXP:
      fwrite(&config_crc32_, sizeof(uint32_t), 1, fd);
      break;
    case CONF_FILE_NXP_RF:
      fwrite(&config_rf_crc32_, sizeof(uint32_t), 1, fd);
      break;
    case CONF_FILE_NXP_TRANSIT:
      fwrite(&config_tr_crc32_, sizeof(uint32_t), 1, fd);
      break;
  }
  fclose(fd);
}

/*******************************************************************************
**
** Function:    CNfcParam::CNfcParam()
**
** Description: class constructor
**
** Returns:     none
**
*******************************************************************************/
CNfcParam::CNfcParam() : m_numValue(0) {}

/*******************************************************************************
**
** Function:    CNfcParam::~CNfcParam()
**
** Description: class destructor
**
** Returns:     none
**
*******************************************************************************/
CNfcParam::~CNfcParam() {}

/*******************************************************************************
**
** Function:    CNfcParam::CNfcParam()
**
** Description: class copy constructor
**
** Returns:     none
**
*******************************************************************************/
CNfcParam::CNfcParam(const char* name, const string& value)
    : string(name), m_str_value(value), m_numValue(0) {}

/*******************************************************************************
**
** Function:    CNfcParam::CNfcParam()
**
** Description: class copy constructor
**
** Returns:     none
**
*******************************************************************************/
CNfcParam::CNfcParam(const char* name, unsigned long value)
    : string(name), m_numValue(value) {}

/*******************************************************************************
**
** Function:    readOptionalConfig()
**
** Description: read Config settings from an optional conf file
**
** Returns:     none
**
*******************************************************************************/
void readOptionalConfig(const char* extra) {
  string strPath;
  string configName(extra_config_base);
  configName += extra;
  configName += extra_config_ext;

  if (alternative_config_path[0] != '\0') {
    strPath.assign(alternative_config_path);
    strPath += configName;
  } else {
    findConfigFilePathFromTransportConfigPaths(configName, strPath);
  }

  CNfcConfig::GetInstance().readConfig(strPath.c_str(), false);
}

/*******************************************************************************
**
** Function:    GetStrValue
**
** Description: API function for getting a string value of a setting
**
** Returns:     True if found, otherwise False.
**
*******************************************************************************/
extern "C" int GetNxpStrValue(const char* name, char* pValue,
                              unsigned long len) {
  CNfcConfig& rConfig = CNfcConfig::GetInstance();

  return rConfig.getValue(name, pValue, len);
}

/*******************************************************************************
**
** Function:    GetByteArrayValue()
**
** Description: Read byte array value from the config file.
**
** Parameters:
**              name - name of the config param to read.
**              pValue  - pointer to input buffer.
**              bufflen - input buffer length.
**              len - out parameter to return the number of bytes read from
**                    config file, return -1 in case bufflen is not enough.
**
** Returns:     TRUE[1] if config param name is found in the config file, else
**              FALSE[0]
**
*******************************************************************************/
extern "C" int GetNxpByteArrayValue(const char* name, char* pValue,
                                    long bufflen, long* len) {
  CNfcConfig& rConfig = CNfcConfig::GetInstance();

  return rConfig.getValue(name, pValue, bufflen, len);
}

/*******************************************************************************
**
** Function:    GetNumValue
**
** Description: API function for getting a numerical value of a setting
**
** Returns:     true, if successful
**
*******************************************************************************/
extern "C" int GetNxpNumValue(const char* name, void* pValue,
                              unsigned long len) {
  if (!pValue) return false;

  CNfcConfig& rConfig = CNfcConfig::GetInstance();
  const CNfcParam* pParam = rConfig.find(name);

  if (pParam == NULL) return false;
  unsigned long v = pParam->numValue();
  if (v == 0 && pParam->str_len() > 0 && pParam->str_len() < 4) {
    const unsigned char* p = (const unsigned char*)pParam->str_value();
    for (size_t i = 0; i < pParam->str_len(); ++i) {
      v *= 256;
      v += *p++;
    }
  }
  switch (len) {
    case sizeof(unsigned long):
      *(static_cast<unsigned long*>(pValue)) = (unsigned long)v;
      break;
    case sizeof(unsigned short):
      *(static_cast<unsigned short*>(pValue)) = (unsigned short)v;
      break;
    case sizeof(unsigned char):
      *(static_cast<unsigned char*>(pValue)) = (unsigned char)v;
      break;
    default:
      return false;
  }
  return true;
}

/*******************************************************************************
**
** Function:    setNxpRfConfigPath
**
** Description: sets the path of the NXP RF config file
**
** Returns:     none
**
*******************************************************************************/
extern "C" void setNxpRfConfigPath(const char* name) {
  memset(nxp_rf_config_path, 0, sizeof(nxp_rf_config_path));
  strlcpy(nxp_rf_config_path, name, sizeof(nxp_rf_config_path));
  ALOGD("nxp_rf_config_path=%s", nxp_rf_config_path);
}

/*******************************************************************************
**
** Function:    setNxpFwConfigPath
**
** Description: sets the path of the NXP FW library
**
** Returns:     none
**
*******************************************************************************/
extern "C" void setNxpFwConfigPath(const char* name) {
  memset(Fw_Lib_Path, 0, sizeof(Fw_Lib_Path));
  strlcpy(Fw_Lib_Path, name, sizeof(Fw_Lib_Path));
  ALOGD("Fw_Lib_Path=%s", Fw_Lib_Path);
}

/*******************************************************************************
**
** Function:    resetConfig
**
** Description: reset settings array
**
** Returns:     none
**
*******************************************************************************/
extern "C" void resetNxpConfig()

{
  CNfcConfig& rConfig = CNfcConfig::GetInstance();

  rConfig.clean();
}

/*******************************************************************************
**
** Function:    isNxpConfigModified()
**
** Description: check if config file has modified
**
** Returns:     0 if not modified, 1 otherwise.
**
*******************************************************************************/
extern "C" int isNxpConfigModified() {
  CNfcConfig& rConfig = CNfcConfig::GetInstance();
  return rConfig.isModified(CONF_FILE_NXP);
}

/*******************************************************************************
**
** Function:    isNxpRFConfigModified()
**
** Description: check if config file has modified
**
** Returns:     0 if not modified, 1 otherwise.
**
*******************************************************************************/
extern "C" int isNxpRFConfigModified() {
  int retRF = 0, rettransit = 0, ret = 0;
  CNfcConfig& rConfig = CNfcConfig::GetInstance();
  retRF = rConfig.isModified(CONF_FILE_NXP_RF);
  rettransit =
      rConfig.isModified(CONF_FILE_NXP_TRANSIT);
  ret = retRF | rettransit;
  ALOGD("ret RF or Transit value %d", ret);
  return ret;
}

/*******************************************************************************
**
** Function:    updateNxpConfigTimestamp()
**
** Description: update if config file has modified
**
** Returns:     0 if not modified, 1 otherwise.
**
*******************************************************************************/
extern "C" int updateNxpConfigTimestamp() {
  CNfcConfig& rConfig = CNfcConfig::GetInstance();
  rConfig.resetModified(CONF_FILE_NXP);
  return 0;
}
/*******************************************************************************
**
** Function:    updateNxpConfigTimestamp()
**
** Description: update if config file has modified
**
** Returns:     0 if not modified, 1 otherwise.
**
*******************************************************************************/
extern "C" int updateNxpRfConfigTimestamp() {
  CNfcConfig& rConfig = CNfcConfig::GetInstance();
  rConfig.resetModified(CONF_FILE_NXP_RF);
  rConfig.resetModified(CONF_FILE_NXP_TRANSIT);
   return 0;
 }
