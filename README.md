# Autoware

Open-source software for autonomous driving

## License

* New BSD License
    * See LICENSE

## How to Build

```
$ cd $HOME
$ git clone git@github.com:CPFL/Autoware.git
$ cd ~/Autoware/ros/src
$ catkin_init_workspace
$ cd ../
$ catkin_make clean # for generating setup scripts
$ source devel/setup.bash # you should load devel/setup.zsh if you are zsh user
$ catkin_make
```

cleanする場合は `catkin_make clean`, ビルドプロセスの詳細を見たい場合, `catkin_make VERBOSE=1`.

## How to Run

### カメラキャリブレーション

参考URL
 - http://ros-robot.blogspot.jp/2010/11/cameracalibrationusb.html

USBカメラを接続し起動

```
 $ rosrun uvc_camera uvc_camera_node
```

チェッカーボードを用意 (例えば checkr2.bmp)

```
 $ rosrun camera_calibration cameracalibrator.py --size 7x3 --square 0.0175 image:=/image_raw
```

ボードを動かしてみて
 - calibrate ボタン有効 click
 - save ボタン有効 click

メッセージ `('Wrote calibration data to', '/tmp/calibrationdata.tar.gz')`を確認

```
 $ tar xzf /tmp/calibrationdata.tar.gz ost.txt
```

ost.txt の内容のうち

  - camera matrix の 9 つの値
  - distortion の 5 つの値のうち末尾の 0 以外の4つの値

を使用する

### 空間キャリブレーション

北陽センサを接続し起動

```
 $ roslaunch hokuyo hokuyo_utm30lx.launch
```

[param.yaml]作成方法は [ReadMe.txt](ros/src/sensing/calibration/packages/camera_lidar2d/ReadMe.txt)を参照

[param.yaml]中の

```
  intrinsic_matirix: !!oencv-matirx
   data [] に ost.txt の camera matrix の 9 つの値を設定
  distrotion_matrix: !!opencv-matrix
   data [] に ost.txt の distortion の 5 つの値のうち末尾の 0 以外の4つの値を設定
```

camera.yaml ファイルの出力先ディレクトリ ~/.ros/camera_info/ が無い場合は作成しておく
```
 $ mkdir -p ~/.ros/camera_info
```

```
 $ rosrun camera_lidar2d camera_lidar2d_offline_calib
```

LRF画面 CALIBRATE(click) SAVE(click)
端末の saved 表示確認して ^C で終了
`~/.ros/camera_info/camera.yaml` が生成される


#### param.yaml のデフォルトのパス

<camera_lidar2d パッケージディレクトリ>/param.yaml


#### camera.yaml のデフォルトのパス

~/.ros/camera_info/camera.yaml 


#### 別の場所にある param.yaml を使用し、別の場所に camera.yaml を出力する場合

```
 $ rosparam set camera_lidar2d/param_yaml ~/other_dir/param.yaml
 $ rosparam set camera_lidar2d/camera_yaml ~/another_dir/camera.yaml
 $ rosrun camera_lidar2d camera_lidar2d_offline_calib
```

#### デフォルトのパスに戻す場合
```
 $ rosparam delete camera_lidar2d/param_yaml
 $ rosparam delete camera_lidar2d/camera_yaml
```


### カメラ起動

```
 $ rosrun uvc_camera uvc_camera_node
```

### 北陽センサ起動

```
 $ roslaunch hokuyo hokuyo_utm30lx.launch
```

### points_to_image ノード起動

```
 $ rosrun scan_to_image scan_to_image
```

#### camera.yaml のデフォルトのパス

~/.ros/camera_info/camera.yaml 


#### manual.yaml のデフォルトのパス

<scan_to_image パッケージディレクトリ>/manual.yaml


#### 別の場所にある camera.yaml manual.yaml を使用する場合

```
 $ rosparam set scan_to_image/camera_yaml ~/other_dir/camera.yaml
 $ rosparam set scan_to_image/manual_yaml ~/another_dir/manual.yaml
 $ rosrun scan_to_image points_to_image
```

#### デフォルトのパスに戻す場合
```
 $ rosparam delete scan_to_image/camera_yaml
 $ rosparam delete scan_to_image/manual_yaml
```


### car_detector(OpenCV)ノードの起動

```
 $ rosrun car_detector car_dpm
```

カメラの画像と矩形、その他が画面に表示される

### car_detector(GPU)ノードの起動

```
 $ rosrun car_detector car_dpm_gpu
```

カメラの画像と矩形、その他が画面に表示される

### pedestrian_detector(OpenCV)ノードの起動

```
 $ rosrun pedestrian_detector pedestrian_dpm
```

カメラの画像と矩形、その他が画面に表示される

### pedestrian_detector(GPU)ノードの起動

```
 $ rosrun pedestrian_detector pedestrian_dpm_gpu
```

カメラの画像と矩形、その他が画面に表示される


#### OpenCV版 car_detector, pedestrian_detectorのデフォルトパラメータ

```c++
  overlapThreshold = 0.5
  numThreads = 6
```

#### OpenCV版 car_detector, pedestrian_detectorのパラメータ変更

```
# 車
 $ rosparam set car_detector/threshold 0.48
 $ rosparam set car_detector/threads 7

# 歩行者
 $ rosparam set pedestrian_detector/threshold 0.48
 $ rosparam set pedestrian_detector/threads 7
```


### fusion_detector ノード起動

```
 $ rosrun fusion fusion_detector
```

カメラの画像と矩形、その他が画面に表示される


### /fused_objects topic出力の確認

```
 $ rostopic echo /fused_objects
```


### デバッグ開発用にダミー画像を送るノードの起動

```
 $ rosrun fake_drivers fake_camera [画像ファイルのパス]
```

画像ファイルの内容を /image_raw topic として出力する

#### フレームレートの変更

デフォルト設定は 30 fps

```
 $ rosparam set fake_camera/fps 15
```

### FlyCapture SDK 設定方法

ライブラリの依存関係

```
 $  sudo apt-get install libraw1394-11 libgtk2.0-0 libgtkmm-2.4-dev libglademm-2.4­dev libgtkglextmm-x11-1.2-dev libusb-1.0-0

```
SDK の設定

* 1. PointGreyウェブサイトからダウンロード（無料アカウントが必要）
* http://vega.ertl.jp/~amonrroy/flycapture2-2.6.3.2-amd64-pkg.tgz

```
sudo sh install_flycapture.sh
```

* 2. 自分のユーザーを入力して許可のグループ追加されます

フィール：/etc/default/grub 開けて 次のライン
             GRUB_CMDLINE_LINUX_DEFAULT="quiet splash"
変わって＝＞ GRUB_CMDLINE_LINUX_DEFAULT="quiet splash usbcore usbfs_memory_mb=1000"

```
sudo update-grub
sudo modprobe usbcore usbfs_memory_mb=1000
```

* 3. 再起動
* 4. カメラの試し方
```
$ cd ~
$ cp -rf /usr/src/flycapture .
$ cd flycapture/src/FlyCapture2Test
$ sudo apt-get install build-essential
$ make
$ cd ~/flycapture/bin
$ ./FlyCapture2Test
```
