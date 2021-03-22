#ifndef __KIRAN_FACE_MSG_H
#define __KIRAN_FACE_MSG_H

#define IMAGE_TYPE 0x60  //图形类型
#define AXIS_TYPE 0x61  //人脸坐标类型

#pragma pack (1)

struct face_image
{
    unsigned char type; //类型
    unsigned int width; //图片宽度
    unsigned int height; //图片高度
    unsigned int len; //图片内容长度
    unsigned char content[0]; //图片内容
};

struct face_axis
{
    unsigned char type; //类型
    unsigned int len; //内容长度
    unsigned char content[0]; //坐标内容
};

#pragma pack ()

#endif /* __KIRAN_FACE_MSG_H */
