/**
 * Copyright (c) 2020 ~ 2021 KylinSec Co., Ltd. 
 * kiran-cc-daemon is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2. 
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2 
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, 
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, 
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.  
 * See the Mulan PSL v2 for more details.  
 * 
 * Author:     wangxiaoqing <wangxiaoqing@kylinos.com.cn>
 */

#ifndef __KIRAN_PAM_MSG_H__
#define __KIRAN_PAM_MSG_H__

#define ASK_FPINT 	"ReqFingerprint" //请求指纹认证界面
#define ASK_FACE  	"ReqFace"        //请求人脸认证界面

#define REP_FPINT       "RepFingerprintReady" //指纹认证界面准备完毕
#define REP_FACE        "RepFaceReady" //人脸认证界面准备完毕

#endif /*__KIRAN_PAM_MSG_H__ */
