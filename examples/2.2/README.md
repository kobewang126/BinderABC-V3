# 2.2 Parcel 编解码

本章只运行用户态 Parcel 编解码示例：

~~~sh
make
./build/parcel_demo
./build/parcel_test
~~~

教学格式使用小端整数和四字节对齐的长度前缀字符串，与 Android Parcel 不兼容；本章尚未把 Parcel 接入 RPC。
