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

#ifndef __KIRAN_FACE_MSG_H
#define __KIRAN_FACE_MSG_H

#define IMAGE_TYPE 0x60           //图形类型
#define AXIS_TYPE 0x61            //人脸坐标类型
#define COMPARE_IMAGE_TYPE 0x62   //图片比较
#define COMPARE_RESULT_TYPE 0x63  //图片比较结果
#define FACE_MATCH 0x01           //人脸匹配
#define FACE_NOT_MATCH 0x02       //人脸不匹配

#pragma pack(1)

struct face_image
{
    unsigned char type;        //类型
    unsigned int channel;      //通道
    unsigned int width;        //图片宽度
    unsigned int height;       //图片高度
    unsigned int len;          //图片内容长度
    unsigned char content[0];  //图片内容
};

struct face_axis
{
    unsigned char type;        //类型
    unsigned int len;          //内容长度
    unsigned char content[0];  //坐标内容
};

struct compare_source
{
    unsigned char type;        //类型
    unsigned int channel;      //通道
    unsigned int width1;       //图片1宽度
    unsigned int height1;      //图片1高度
    unsigned int len1;         //图片1内容长度
    unsigned int width2;       //图片2宽度
    unsigned int height2;      //图片2高度
    unsigned int len2;         //图片2内容长度
    unsigned char content[0];  //两张图片内容，图片1方前面，图片2方后面
};

struct compare_result
{
    unsigned char type;    //类型
    unsigned char result;  //结果
};

#pragma pack()

#endif /* __KIRAN_FACE_MSG_H */
