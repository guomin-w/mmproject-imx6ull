# 基于 QT、V4L2、ALSA、FFmpeg 的嵌入式 Linux 项目，具有相机、音乐播放、视频播放功能的多媒体系统
## 主界面
- mainwidget 包含四个功能按钮，点击任一按钮可打开相应的功能界面
![mainwidget](/result/mainwidget.jpg)

## 相机
- camerawidget 相机功能界面，一键打开/关闭 USB 摄像头，可实时显示当前摄像头画面，单击拍照可以保存照片
- cameracapture 摄像头数据捕获线程，基于 V4L2 框架，将捕获的图像数据以信号的形式发送到上层界面
![camerawidget_off](/result/camerawidget_off.jpg)
![camerawidget_on](/result/camerawidget_on.jpg)

## 相册
- albumwidget 相册功能界面，展示照片及其信息，通过左右两边按钮可切换显示照片
![albumwidget](/result/albumwidget.jpg)

## 音乐播放
- musicwidget 音乐功能界面，可查看音乐文件列表，实现播放/暂停、上下曲切换、单击列表项切换、进度条显示并可拖动跳转、循环播放模式切换 (列表循环、单曲循环、随机播放)、音量调节 (单击按钮可弹出调节条)
- musicplay 音乐播放子线程，基于 ALSA 框架控制 pcm 和混音器设备，基于 FFmpeg 解析音乐文件并转化为 imx6ull 支持的数据格式，将数据写入 pcm 设备实现播放
![musicwidget](/result/musicwidget.jpg)

## 视频播放
- videowidget 视频功能界面，可查看视频文件列表，实现播放/暂停、视频切换、单击列表切换、进度条、音量调节，从共享内存中得到视频帧画面，借助音频帧时间戳实现音视频同步
- videoqueue 视频帧数据包队列结构的实现，创建了一个数据包队列类，实现了线程安全的数据包入队、出队、清空
- videoplay 解复用子线程，基于 FFmpeg 从视频文件中解析出视频流和音频流的数据包，分别放入视频帧队列和音频帧队列
- videoplayaudio 音频数据包解码子线程，基于 FFmpeg 解码音频数据包 (从音频数据包队列中取出)，计算音频帧时钟作为音视频同步时钟，基于 ALSA 框架将解码后的数据写入 pcm 设备实现音频播放
- videoplayvideo 视频数据包解码子线程，基于 FFmpeg 解码视频数据包 (从视频数据包队列中取出)，依据音频时钟调节视频帧，将解码后的视频帧数据写入共享内存中，发送信号通知上层刷新视频帧
![videowidget](/result/videowidget.jpg)
