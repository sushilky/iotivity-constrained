/****************************************************************************
 *
 * Copyright 2018 Samsung Electronics All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 *
 ****************************************************************************/

#include "st_data_manager.h"
#include "oc_rep.h"
#include "oc_ri.h"
#include "port/oc_storage.h"
#include "st_port.h"
#include "util/oc_mem.h"

#define ST_MAX_DATA_SIZE (2048)
#define ST_MAX_STR_LEN (100)
#define ST_DATA_MANAGER_NAME "st_device_info"

#define ST_DEVICE_KEY "device"

#define ST_SPECIFICATION_KEY "specification"
#define ST_SPEC_DEVICE_KEY "device"
#define ST_SPEC_PLATFORM_KEY "platform"
#define ST_SPEC_DEVICE_TYPE_KEY "deviceType"
#define ST_SPEC_DEVICE_NAME_KEY "deviceName"
#define ST_SPEC_SPEC_VER_KEY "specVersion"
#define ST_SPEC_DATA_MODEL_VER_KEY "dataModelVersion"
#define ST_SPEC_MF_NAME_KEY "manufacturerName"
#define ST_SPEC_MF_URL_KEY "manufacturerUrl"
#define ST_SPEC_MF_DATE_KEY "manufacturingDate"
#define ST_SPEC_MODEL_NUMBER_KEY "modelNumber"
#define ST_SPEC_PLATFORM_VER_KEY "platformVersion"
#define ST_SPEC_OS_VER_KEY "osVersion"
#define ST_SPEC_HARD_VER_KEY "hardwareVersion"
#define ST_SPEC_FIRM_VER_KEY "firmwareVersion"
#define ST_SPEC_VENDER_ID_KEY "vendorId"

#define ST_RESOURCES_KEY "resources"
#define ST_RSC_SINGLE_KEY "single"
#define ST_RSC_URI_KEY "uri"
#define ST_RSC_TYPES_KEY "types"
#define ST_RSC_INTERFACES_KEY "interfaces"
#define ST_RSC_POLICY_KEY "policy"

#define ST_RESOURCE_TYPES_KEY "resourceTypes"

#define ST_RSC_TYPES_TYPE_KEY "type"
#define ST_RSC_TYPES_PROPS_KEY "properties"
#define ST_PROPS_KEY_KEY "key"
#define ST_PROPS_TYPE_KEY "type"
#define ST_PROPS_MANDATORY_KEY "mandatory"
#define ST_PROPS_RW_KEY "rw"

#define st_rep_set_string_with_chk(object, key, value)                         \
  if (value)                                                                   \
    oc_rep_set_text_string(object, key, value);

#define st_string_check_free(str)                                              \
  if (oc_string(*str))                                                         \
  oc_free_string(str)

#define st_string_check_new(str, value, size)                                  \
  if (value && size > 0)                                                       \
  oc_new_string(str, value, size)

OC_LIST(st_specification_list);
OC_MEMB(st_specification_s, st_specification_t, OC_MAX_NUM_DEVICES);

OC_LIST(st_resource_list);
OC_MEMB(st_resource_s, st_resource_info_t,
        OC_MAX_APP_RESOURCES *OC_MAX_NUM_DEVICES);

OC_LIST(st_resource_type_list);
OC_MEMB(st_resource_type_s, st_resource_type_t,
        MAX_NUM_PROPERTIES *OC_MAX_APP_RESOURCES *OC_MAX_NUM_DEVICES);

static int st_decode_device_data_info(oc_rep_t *rep);

#ifdef OC_DYNAMIC_ALLOCATION
static unsigned char *g_device_def = NULL;
static unsigned int g_device_def_len = 0;
#else
static unsigned char g_device_def[ST_MAX_DATA_SIZE];
static unsigned int g_device_def_len = 0;
#endif

int
st_data_mgr_info_load(void)
{
  int ret = 0;
  oc_rep_t *rep;

  if (g_device_def_len > 0) {
#ifndef OC_DYNAMIC_ALLOCATION
    char rep_objects_alloc[OC_MAX_NUM_REP_OBJECTS];
    oc_rep_t rep_objects_pool[OC_MAX_NUM_REP_OBJECTS];
    memset(rep_objects_alloc, 0, OC_MAX_NUM_REP_OBJECTS * sizeof(char));
    memset(rep_objects_pool, 0, OC_MAX_NUM_REP_OBJECTS * sizeof(oc_rep_t));
    struct oc_memb rep_objects = { sizeof(oc_rep_t), OC_MAX_NUM_REP_OBJECTS,
                                   rep_objects_alloc, (void *)rep_objects_pool,
                                   0 };
#else  /* !OC_DYNAMIC_ALLOCATION */
    struct oc_memb rep_objects = { sizeof(oc_rep_t), 0, 0, 0, 0 };
#endif /* OC_DYNAMIC_ALLOCATION */
    oc_rep_set_pool(&rep_objects);
    oc_parse_rep(g_device_def, (uint16_t)g_device_def_len, &rep);
    ret = st_decode_device_data_info(rep);
    oc_free_rep(rep);
  } else {
    st_print_log("[ST_DATA_MGR] can't read device info\n");
    return -1;
  }

  return ret;
}

st_specification_t *
st_data_mgr_get_spec_info(void)
{
  if (oc_list_length(st_specification_list) > 0) {
    return (st_specification_t *)oc_list_head(st_specification_list);
  } else {
    return NULL;
  }
}

st_resource_info_t *
st_data_mgr_get_resource_info(void)
{
  if (oc_list_length(st_resource_list) > 0) {
    return (st_resource_info_t *)oc_list_head(st_resource_list);
  } else {
    return NULL;
  }
}

st_resource_type_t *
st_data_mgr_get_rsc_type_info(const char *rt)
{
  st_resource_type_t *rt_info = oc_list_head(st_resource_type_list);

  size_t rt_len = strlen(rt);
  while (rt_info != NULL &&
         (rt_len != oc_string_len(rt_info->type) ||
          strncmp(rt, oc_string(rt_info->type), rt_len) != 0)) {
    rt_info = rt_info->next;
  }

  if (!rt_info) {
    st_print_log("[ST_DATA_MGR] can't find %s resource type info\n", rt);
    return NULL;
  }

  st_print_log("[ST_DATA_MGR] find %s resource type info\n", rt);
  return rt_info;
}

static void
free_specifications_device(st_device_info_t *device)
{
  if(!device) return;
  st_string_check_free(&device->device_type);
  st_string_check_free(&device->device_name);
  st_string_check_free(&device->spec_version);
  st_string_check_free(&device->data_model_version);
}

static void
free_specifications_platform(st_platform_info_t *platform)
{
  if(!platform) return;
  st_string_check_free(&platform->manufacturer_name);
  st_string_check_free(&platform->manufacturer_uri);
  st_string_check_free(&platform->manufacturing_date);
  st_string_check_free(&platform->model_number);
  st_string_check_free(&platform->platform_version);
  st_string_check_free(&platform->os_version);
  st_string_check_free(&platform->hardware_version);
  st_string_check_free(&platform->firmware_version);
  st_string_check_free(&platform->vendor_id);
}

static void
free_specifications(void)
{
  st_specification_t *item = oc_list_head(st_specification_list), *next;
  while (item != NULL) {
    next = item->next;
    oc_list_remove(st_specification_list, item);
    free_specifications_device(&item->device);
    free_specifications_platform(&item->platform);
    oc_memb_free(&st_specification_s, item);
    item = next;
  }
}

static void
free_resources(void)
{
  st_resource_info_t *item = oc_list_head(st_resource_list), *next;
  while (item != NULL) {
    next = item->next;
    oc_list_remove(st_resource_list, item);
    st_string_check_free(&item->uri);
    if (oc_string_array_get_allocated_size(item->types) > 0) {
      oc_free_string_array(&item->types);
    }
    oc_memb_free(&st_resource_s, item);
    item = next;
  }
}

static void
free_resource_types(void)
{
  st_resource_type_t *item = oc_list_head(st_resource_type_list), *next;
  while (item != NULL) {
    next = item->next;
    oc_list_remove(st_resource_type_list, item);
    st_string_check_free(&item->type);
#ifdef OC_DYNAMIC_ALLOCATION
    st_property_t *cur_prop = oc_list_head(item->properties), *next_prop;
    while (cur_prop) {
      next_prop = cur_prop->next;
      oc_list_remove(item->properties, cur_prop);
      st_string_check_free(&cur_prop->key);
      oc_mem_free(cur_prop);
      cur_prop = next_prop;
    }
#else  /* OC_DYNAMIC_ALLOCATION */
    int i;
    for (i = 0; i < item->prperties_cnt; i++) {
      st_string_check_free(&item->properties[i].key);
    }
#endif /* !OC_DYNAMIC_ALLOCATION */
    oc_memb_free(&st_resource_type_s, item);
    item = next;
  }
}

void
st_data_mgr_info_free(void)
{
  free_specifications();
  free_resources();
  free_resource_types();
}

static int
st_decode_spec(int device_index, oc_rep_t *spec_rep)
{
  st_specification_t *spec_info = oc_memb_alloc(&st_specification_s);
  if (!spec_info) {
    st_print_log("[ST_DATA_MGR] alloc failed\n");
    return -1;
  }
  spec_info->device_idx = device_index;

  char *value = NULL;
  int size = 0;
  oc_rep_t *spec_device_rep = NULL;
  if (oc_rep_get_object(spec_rep, ST_SPEC_DEVICE_KEY, &spec_device_rep)) {
    if (oc_rep_get_string(spec_device_rep, ST_SPEC_DEVICE_TYPE_KEY, &value,
                          &size)) {
      st_string_check_new(&spec_info->device.device_type, value, size);
    }
    if (oc_rep_get_string(spec_device_rep, ST_SPEC_DEVICE_NAME_KEY, &value,
                          &size)) {
      st_string_check_new(&spec_info->device.device_name, value, size);
    }
    if (oc_rep_get_string(spec_device_rep, ST_SPEC_SPEC_VER_KEY, &value,
                          &size)) {
      st_string_check_new(&spec_info->device.spec_version, value, size);
    }
    if (oc_rep_get_string(spec_device_rep, ST_SPEC_DATA_MODEL_VER_KEY, &value,
                          &size)) {
      st_string_check_new(&spec_info->device.data_model_version, value, size);
    }
  } else {
    st_print_log("[ST_DATA_MGR] can't get specification device data\n");
    return -1;
  }

  oc_rep_t *spec_platform_rep = NULL;
  if (oc_rep_get_object(spec_rep, ST_SPEC_PLATFORM_KEY, &spec_platform_rep)) {
    if (oc_rep_get_string(spec_platform_rep, ST_SPEC_MF_NAME_KEY, &value,
                          &size)) {
      st_string_check_new(&spec_info->platform.manufacturer_name, value, size);
    }
    if (oc_rep_get_string(spec_platform_rep, ST_SPEC_MF_URL_KEY, &value,
                          &size)) {
      st_string_check_new(&spec_info->platform.manufacturer_uri, value, size);
    }
    if (oc_rep_get_string(spec_platform_rep, ST_SPEC_MF_DATE_KEY, &value,
                          &size)) {
      st_string_check_new(&spec_info->platform.manufacturing_date, value, size);
    }
    if (oc_rep_get_string(spec_platform_rep, ST_SPEC_MODEL_NUMBER_KEY, &value,
                          &size)) {
      st_string_check_new(&spec_info->platform.model_number, value, size);
    }
    if (oc_rep_get_string(spec_platform_rep, ST_SPEC_PLATFORM_VER_KEY, &value,
                          &size)) {
      st_string_check_new(&spec_info->platform.platform_version, value, size);
    }
    if (oc_rep_get_string(spec_platform_rep, ST_SPEC_OS_VER_KEY, &value,
                          &size)) {
      st_string_check_new(&spec_info->platform.os_version, value, size);
    }
    if (oc_rep_get_string(spec_platform_rep, ST_SPEC_HARD_VER_KEY, &value,
                          &size)) {
      st_string_check_new(&spec_info->platform.hardware_version, value, size);
    }
    if (oc_rep_get_string(spec_platform_rep, ST_SPEC_FIRM_VER_KEY, &value,
                          &size)) {
      st_string_check_new(&spec_info->platform.firmware_version, value, size);
    }
    if (oc_rep_get_string(spec_platform_rep, ST_SPEC_VENDER_ID_KEY, &value,
                          &size)) {
      st_string_check_new(&spec_info->platform.vendor_id, value, size);
    }
  } else {
    st_print_log("[ST_DATA_MGR] can't get specification platform data\n");
    return -1;
  }

  oc_list_add(st_specification_list, spec_info);

  return 0;
}

static int
st_decode_resources(int device_index, oc_rep_t *resources_rep)
{
  oc_rep_t *single_rep;
  if (oc_rep_get_object_array(resources_rep, ST_RSC_SINGLE_KEY, &single_rep)) {
    oc_rep_t *iter = single_rep;
    while (iter) {
      oc_rep_t *item = iter->value.object;
      st_resource_info_t *resource_info = oc_memb_alloc(&st_resource_s);
      if (!resource_info) {
        st_print_log("[ST_DATA_MGR] alloc failed\n");
        return -1;
      }
      resource_info->device_idx = device_index;

      char *value = NULL;
      int size = 0;
      if (oc_rep_get_string(item, ST_RSC_URI_KEY, &value, &size)) {
        st_string_check_new(&resource_info->uri, value, size);
      }

      oc_string_array_t array_value;
      if (oc_rep_get_string_array(item, ST_RSC_TYPES_KEY, &array_value,
                                  &size)) {
        oc_new_string_array(&resource_info->types, size);
        int i = 0;
        for (i = 0; i < size; i++) {
          value = oc_string_array_get_item(array_value, i);
          oc_string_array_add_item(resource_info->types, value);
        }
      }

      if (oc_rep_get_string_array(item, ST_RSC_INTERFACES_KEY, &array_value,
                                  &size)) {
        int i = 0;
        resource_info->interfaces = 0;
        resource_info->default_interface = 0;
        for (i = 0; i < size; i++) {
          value = oc_string_array_get_item(array_value, i);
          resource_info->interfaces |= oc_ri_get_interface_mask(
            oc_string_array_get_item(array_value, i),
            oc_string_array_get_item_size(array_value, i));
          if (i == 0) {
            resource_info->default_interface = resource_info->interfaces;
          }
        }
      }

      int policy = 0;
      if (oc_rep_get_int(item, ST_RSC_POLICY_KEY, &policy)) {
        resource_info->policy = policy;
      }

      oc_list_add(st_resource_list, resource_info);
      iter = iter->next;
    }
  } else {
    st_print_log("[ST_DATA_MGR] don't have exist resources\n");
    return -1;
  }

  return 0;
}

static int
st_decode_device(int device_idx, oc_rep_t *device_rep)
{
  oc_rep_t *spec_rep = NULL;
  if (oc_rep_get_object(device_rep, ST_SPECIFICATION_KEY, &spec_rep)) {
    if (st_decode_spec(device_idx, spec_rep) != 0) {
      st_print_log("[ST_DATA_MGR] st_decode_spec failed\n");
      return -1;
    }
  } else {
    st_print_log("[ST_DATA_MGR] can't get specification data\n");
    return -1;
  }

  oc_rep_t *resources_rep = NULL;
  if (oc_rep_get_object(device_rep, ST_RESOURCES_KEY, &resources_rep)) {
    if (st_decode_resources(device_idx, resources_rep) != 0) {
      st_print_log("[ST_DATA_MGR] st_decode_resources failed\n");
      return -1;
    }
  } else {
    st_print_log("[ST_DATA_MGR] can't get resources data\n");
    return -1;
  }

  return 0;
}

static int
st_decode_resource_types(oc_rep_t *rsc_type_rep)
{
  st_resource_type_t *rt = oc_memb_alloc(&st_resource_type_s);
  if (!rt) {
    st_print_log("[ST_DATA_MGR] alloc failed\n");
    return -1;
  }

  char *value = NULL;
  int size = 0;
  if (oc_rep_get_string(rsc_type_rep, ST_RSC_TYPES_TYPE_KEY, &value, &size)) {
    st_string_check_new(&rt->type, value, size);
  }

  oc_rep_t *properties_rep = NULL;
  if (oc_rep_get_object_array(rsc_type_rep, ST_RSC_TYPES_PROPS_KEY,
                              &properties_rep)) {
    oc_rep_t *iter = properties_rep;
#ifdef OC_DYNAMIC_ALLOCATION
    OC_LIST_STRUCT_INIT(rt, properties);
#else  /* OC_DYNAMIC_ALLOCATION */
    rt->properties_cnt = 0;
#endif /* !OC_DYNAMIC_ALLOCATION */
    while (iter) {
      oc_rep_t *item = iter->value.object;
#ifdef OC_DYNAMIC_ALLOCATION
      st_property_t *property = oc_mem_malloc(sizeof(st_property_t));
      if (!property) {
        st_print_log("[ST_DATA_MGR] alloc failed\n");
        return -1;
      }
#else  /* OC_DYNAMIC_ALLOCATION */
      if (rt->properties_cnt >= MAX_NUM_PROPERTIES) {
        st_print_log("[ST_DATA_MGR] properties overflow\n");
        return -1;
      }
      st_property_t *property = rt->properties[rt->properties_cnt];
      rt->properties_cnt++;
#endif /* !OC-DYNAMIC_ALLOCATION */
      if (oc_rep_get_string(item, ST_PROPS_KEY_KEY, &value, &size)) {
        st_string_check_new(&property->key, value, size);
      }

      int type;
      if (oc_rep_get_int(item, ST_PROPS_TYPE_KEY, &type)) {
        property->type = type;
      }

      bool mandatory;
      if (oc_rep_get_bool(item, ST_PROPS_MANDATORY_KEY, &mandatory)) {
        property->mandatory = mandatory;
      }

      int rw;
      if (oc_rep_get_int(item, ST_PROPS_RW_KEY, &rw)) {
        property->rw = rw;
      }

#ifdef OC_DYNAMIC_ALLOCATION
      oc_list_add(rt->properties, property);
#endif
      iter = iter->next;
    }
  } else {
    st_print_log("[ST_DATA_MGR] can't get resource type data\n");
    return -1;
  }

  oc_list_add(st_resource_type_list, rt);

  return 0;
}

static int
st_decode_device_data_info(oc_rep_t *rep)
{
  oc_rep_t *device_rep = NULL;
  if (oc_rep_get_object_array(rep, ST_DEVICE_KEY, &device_rep)) {
    oc_rep_t *iter = device_rep;
    int i;
    for (i = 0; iter != NULL; iter = iter->next, i++) {
      oc_rep_t *item = iter->value.object;
      if (st_decode_device(i, item) != 0) {
        st_print_log("[ST_DATA_MGR] can't decode device(%d) data\n", i);
        return -1;
      }
    }
  } else {
    st_print_log("[ST_DATA_MGR] can't get device data\n");
    return -1;
  }

  oc_rep_t *rsc_type_rep = NULL;
  if (oc_rep_get_object_array(rep, ST_RESOURCE_TYPES_KEY, &rsc_type_rep)) {
    oc_rep_t *iter = rsc_type_rep;
    while (iter) {
      oc_rep_t *item = iter->value.object;
      if (st_decode_resource_types(item) != 0) {
        st_print_log("[ST_DATA_MGR] can't decode resource type data\n");
        return -1;
      }
      iter = iter->next;
    }
  } else {
    st_print_log("[ST_DATA_MGR] can't get resource type data\n");
    return -1;
  }

  return 0;
}

bool
st_set_device_profile(unsigned char *device_def, unsigned int device_def_len)
{
  if (!device_def) {
    st_print_log("[ST_DATA_MGR] device_def is NULL \n");
    return false;
  }
  if (!device_def_len) {
    st_print_log("[ST_DATA_MGR] device_def_len is zero \n");
    return false;
  }
#ifdef OC_DYNAMIC_ALLOCATION
  if (g_device_def) {
    oc_mem_free(g_device_def);
    g_device_def = NULL;
  }

  g_device_def_len = device_def_len;
  g_device_def = oc_mem_calloc(g_device_def_len, sizeof(unsigned char));

#else
  if (device_def_len >= ST_MAX_DATA_SIZE) {
    st_print_log("[ST_DATA_MGR] device_def_size should be less than %d bytes\n",
                 ST_MAX_DATA_SIZE);
    return false;
  }
  g_device_def_len = device_def_len;
  memset(g_device_def, 0, sizeof(unsigned char) * ST_MAX_DATA_SIZE);
#endif

  memcpy(g_device_def, device_def, g_device_def_len);

  return true;
}

void
st_free_device_profile(void)
{
#ifdef OC_DYNAMIC_ALLOCATION
  if (g_device_def) {
    oc_mem_free(g_device_def);
    g_device_def = NULL;
  }
#endif
  g_device_def_len = 0;
}