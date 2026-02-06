#/bin/sh

#############prepare dependence library##################
#sudo add-apt-repository ppa:team-gcc-arm-embedded/ppa -y
#sudo apt-get -q update
#sudo apt-get install -y gcc-arm-embedded
#sudo apt-get install -y gcc-aarch64-linux-gnu
#sudo apt-get install build-essential -y
#sudo apt-get install llibncurses-dev -y
#sudo apt-get install gcc-aarch64-linux-gnu -y
#sudo apt-get install bison flex -y
#sudo apt-get install device-tree-compiler -y
#sudo apt-get install u-boot-tools -y
#sudo apt-get install python3 python-is-python3 -y
#sudo apt-get install libc6 -y

#############prepare uboot################
if [ ! -d "./environment" ];then
  mkdir environment
fi


#cd environment
#if [ ! -f "./u-boot-2020.01.tar.bz2" ];then
#    wget ftp://ftp.denx.de/pub/u-boot/u-boot-2020.01.tar.bz2
#    tar vxf u-boot-2020.01.tar.bz2
#fi


export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-gnu-
#cd u-boot-2020.01
#make rpi_4_defconfig
#make all
#cd ../../


####################build XVisor#################

VERBOSE_INFO=0

#使用generic-v8-defconfig配置
#可能拷来拷去，不同OS依赖不同，删除已生成的所有文件，每次保持干净环境
rm -rf build/
find -name "*.o" |xargs rm
find -name "*.sh" |xargs chmod a+x
find -name "*.py" |xargs chmod a+x
rm tools/openconf/conf
rm tools/openconf/.depend
rm tools/openconf/lex.zconf.c
rm tools/openconf/zconf.hash.c

make ARCH=arm generic-v8-defconfig VERBOSE=${VERBOSE_INFO}

#这里生成vmm.bin和uvmm.bin
make  VERBOSE=${VERBOSE_INFO}
mkimage -A arm64 -O linux -T kernel -C none -a 0x00080000 -e 0x00080000 -n Xvisor -d build/vmm.bin build/uvmm.bin
make -C tests/arm64/virt-v8/basic  VERBOSE=${VERBOSE_INFO}

#################prepare linux################

