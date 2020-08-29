#include "camera.hpp"

#include <cstring>
#include <exception>
#include <string>
#include <thread>

#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG

#include "spdlog/spdlog.h"

void Camera::WorkThread() {
  SPDLOG_DEBUG("[Camera] [WorkThread] Running.");

  int err = MV_OK;
  MV_FRAME_OUT frame_out;
  std::memset(&frame_out, 0, sizeof(MV_FRAME_OUT));
  while (continue_capture_) {
    err = MV_CC_GetImageBuffer(camera_handle_, &frame_out, 1000);
    if (err == MV_OK) {
      SPDLOG_INFO("Get One Frame: Width{d}, Height{d}, nFrameNum{d}\n",
                  frame_out.stFrameInfo.nWidth, frame_out.stFrameInfo.nHeight,
                  frame_out.stFrameInfo.nFrameNum);
    } else {
      SPDLOG_ERROR("GetImageBuffer fail! err:{x}\n", err);
    }
    if (NULL != frame_out.pBufAddr) {
      err = MV_CC_FreeImageBuffer(camera_handle_, &frame_out);
      if (err != MV_OK) {
        SPDLOG_ERROR("FreeImageBuffer fail! err:{x}\n", err);
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  SPDLOG_DEBUG("[Camera] [WorkThread] Running.");
}

void Camera::PrintDeviceInfo() {
  if (NULL == mv_dev_info_) {
    SPDLOG_ERROR("[Camera] The Pointer of mv_dev_info_ is NULL!\n");
    return;
  }
  if (mv_dev_info_->nTLayerType == MV_USB_DEVICE) {
    SPDLOG_INFO(
        "[Camera] UserDefinedName: {} \nSerial Number: {}\nDevice Number: {}",
        mv_dev_info_->SpecialInfo.stUsb3VInfo.chUserDefinedName,
        mv_dev_info_->SpecialInfo.stUsb3VInfo.chSerialNumber,
        mv_dev_info_->SpecialInfo.stUsb3VInfo.nDeviceNumber);
  } else {
    SPDLOG_WARN("[Camera] Not support.");
  }
}

void Camera::Prepare() {
  int err = MV_OK;
  std::string err_msg;
  SPDLOG_DEBUG("[Camera] Prepare.");

  std::memset(&mv_dev_list_, 0, sizeof(MV_CC_DEVICE_INFO_LIST));
  err = MV_CC_EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE, &mv_dev_list_);
  if (err != MV_OK) {
    err_msg = "[Camera] EnumDevices fail! err: " + std::to_string(err);
    SPDLOG_ERROR(err_msg);
    throw std::runtime_error(err_msg);
  }

  if (mv_dev_list_.nDeviceNum > 0) {
    for (unsigned int i = 0; i < mv_dev_list_.nDeviceNum; i++) {
      SPDLOG_INFO("[Camera] Device {d} slected.", i);
      mv_dev_info_ = mv_dev_list_.pDeviceInfo[i];
      if (mv_dev_info_ == nullptr) {
        SPDLOG_ERROR("[Camera] Error Reading mv_dev_info_");
        throw std::runtime_error("[Camera] Error Reading mv_dev_info_");
      } else
        PrintDeviceInfo();
    }
  } else {
    SPDLOG_ERROR("[Camera] Find No Devices!");
    throw std::runtime_error("[Camera] Find No Devices!");
  }
}

Camera::Camera() {
  SPDLOG_DEBUG("[Camera] Constructing.");
  Prepare();
  SPDLOG_DEBUG("[Camera] Constructed.");
}

Camera::Camera(unsigned int index) {
  SPDLOG_DEBUG("[Camera] Constructing.");
  Prepare();
  Open(index);
  SPDLOG_DEBUG("[Camera] Constructed.");
}

Camera::~Camera() {
  SPDLOG_DEBUG("[Camera] Destructing.");
  Close();
  SPDLOG_DEBUG("[Camera] Destructed.");
}

void Camera::Open(unsigned int index) {
  int err = MV_OK;
  std::string err_msg;

  SPDLOG_DEBUG("[Camera] Open index:{}.", index);

  if (index >= mv_dev_list_.nDeviceNum) {
    SPDLOG_ERROR("[Camera] Intput index:{} >= nDeviceNum:{} !", index,
                 mv_dev_list_.nDeviceNum);
    throw std::range_error("[Camera] Index range error!");
  }

  err = MV_CC_CreateHandle(&camera_handle_, mv_dev_list_.pDeviceInfo[index]);
  if (err != MV_OK) {
    err_msg = "[Camera] CreateHandle fail! err: " + std::to_string(err);
    SPDLOG_ERROR(err_msg);
    throw std::runtime_error(err_msg);
  }

  err = MV_CC_OpenDevice(camera_handle_);
  if (err != MV_OK) {
    err_msg = "[Camera] OpenDevice fail! err: " + std::to_string(err);
    SPDLOG_ERROR(err_msg);
    throw std::runtime_error(err_msg);
  }

  err = MV_CC_SetEnumValue(camera_handle_, "TriggerMode", 0);
  if (err != MV_OK) {
    err_msg = "[Camera] SetTrigger Mode fail! err: " + std::to_string(err);
    SPDLOG_ERROR(err_msg);
    throw std::runtime_error(err_msg);
  }

  memset(&init_val_, 0, sizeof(MVCC_INTVALUE));
  err = MV_CC_GetIntValue(camera_handle_, "PayloadSize", &init_val_);
  if (err != MV_OK) {
    err_msg = "[Camera] GetPayloadSize fail! err: " + std::to_string(err);
    SPDLOG_ERROR(err_msg);
    throw std::runtime_error(err_msg);
  }

  err = MV_CC_StartGrabbing(camera_handle_);
  if (err != MV_OK) {
    err_msg = "[Camera] StartGrabbing fail! err: " + std::to_string(err);
    SPDLOG_ERROR(err_msg);
    throw std::runtime_error(err_msg);
  }

  continue_capture_ = true;
  capture_thread_ = std::thread(&Camera::WorkThread, this);
}

bool Camera::GetFrame(void* output) { return false; }

int Camera::Close() {
  int err = MV_OK;
  std::string err_msg;

  SPDLOG_DEBUG("[Camera] Close.");

  continue_capture_ = false;
  capture_thread_.join();

  err = MV_CC_StopGrabbing(camera_handle_);
  if (err != MV_OK) {
    SPDLOG_ERROR("[Camera] StopGrabbing fail! err:{x}", err);
    return err;
  }

  err = MV_CC_CloseDevice(camera_handle_);
  if (err != MV_OK) {
    SPDLOG_ERROR("[Camera] ClosDevice fail! err:{x}", err);
    return err;
  }

  err = MV_CC_DestroyHandle(camera_handle_);
  if (err != MV_OK) {
    SPDLOG_ERROR("[Camera] DestroyHandle fail! err:{x}", err);
    return err;
  }

  SPDLOG_DEBUG("[Camera] Closed.");
  return MV_OK;
}
