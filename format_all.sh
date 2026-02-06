#!/bin/bash
#find . -type f \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp"  -o -name "*.c" \) | xargs clang-format --style=file:/media/huangbin/ae65fa29-c04a-464a-b83d-486999a7c8bc/tools/VSCode-linux-x64/data/tools/.clang-format  -i


#find . -type f \( -name "*.c" -o -name "*.cpp" -o -name "*.h" -o -name "*.hpp" -o -name "*.cxx" \) -exec clang-format -style=file:/media/huangbin/ae65fa29-c04a-464a-b83d-486999a7c8bc/tools/VSCode-linux-x64/data/tools/.clang-format  -i {} \;

#sudo apt install clang-format -y
config="./.clang-format"
astyle_config="./astyle.config"

# 查找所有符合条件的文件
files=`find ./ -type f \( -name "*.c" -o -name "*.cpp" -o -name "*.h" -o -name "*.hpp" -o -name "*.cxx" \) `
file_array=($files)
# 使用 for...in 循环格式化每个文件
echo "file_array num1:${#file_array[*]}"
echo "file_array num2:${#file_array[@]}"

for ((i=0;i<${#file_array[@]};i++)) do
    astyle  -n --options=${astyle_config} ${file_array[i]}
    echo clang-format --style=file:"$config" -i ${file_array[i]}
    clang-format --style=file:"$config" -i ${file_array[i]}
done
