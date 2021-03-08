#! /usr/bin/env bash

BASH_SOURCE_REAL_PATH=$(readlink -m ${BASH_SOURCE})
BASH_SOURCE_DIR=$(dirname ${BASH_SOURCE_REAL_PATH})

THIS_CMD_BASENAME=$(basename $0)
ABOUT_THIS="\
convert from xsa to dtbo
Usage: ${THIS_CMD_BASENAME} <-e xsct_exe_file> <-i vivado_xsa_file> <-o output_dir>

note:
## git clone --branch xilinx-v2019.2 https://github.com/Xilinx/device-tree-xlnx
## xsct version must match the branch of device-tree-xlnx
## dtc -O dtb -o pl.dtbo -b 0 -@ pl.dtsi

Example:
${THIS_CMD_BASENAME} -e /tools/petalinux/tools/xsct/bin/xsct -i design_1_wrapper.xsa -o output
"

ORI_ARGS=( "$@" )

INPUT_FILE=design_1_wrapper.xsa
TEMP_DIR=$(pwd)/output
EXE_FILE=/tools/petalinux/tools/xsct/bin/xsct

FIRMWARE_BASE_NAME="altel"

while getopts ":i:e:o:n:" opt; do
    case $opt in
        i)
	    INPUT_FILE="$OPTARG"
	    ;;
        e)
	    EXE_FILE="$OPTARG"
	    ;;
        o)
	    TEMP_DIR="$OPTARG"
	    ;;
        n)
	    FIRMWARE_BASE_NAME="$OPTARG"
	    ;;
	\?)
	    echo "Invalid option: -$OPTARG"
	    printf "$ABOUT_THIS"
	    exit 1
	    ;;
	:)
	    echo "Option -$OPTARG requires an argument"
	    printf "$ABOUT_THIS"
	    exit 1
	    ;;
    esac
done

mkdir -p ${TEMP_DIR}
TEMP_FILE=${TEMP_DIR}/xsa2dtbo.tcl

INPUT_FILE_NAME=$(basename $INPUT_FILE)
cp ${INPUT_FILE} ${TEMP_DIR}

INPUT_FILE=${TEMP_DIR}/${INPUT_FILE_NAME}
INPUT_FILE_REAL_PATH=$(readlink -m $INPUT_FILE)

SUBMIT_TEMPLATE='
## Zynq7000 ps7_cortexa9_0  ;   ZynqUtraScale psu_cortexa53_0
## git clone https://github.com/Xilinx/device-tree-xlnx
## xsct version must match the branch of device-tree-xlnx
# /tools/petalinux/tools/xsct/bin/xsct dt_overlay.tcl  /work/vivado/dma1211/ahcal-axidmatcp/zybo/axidma/design_1_wrapper.xsa  ps7_cortexa9_0 /work/vivado/device-tree-xlnx/  /work/vivado/dts

set xml_path [lindex $argv 0]
set proc_1 [lindex $argv 1]
set repo_path [lindex $argv 2]
set out_dir [lindex $argv 3]

puts "*********************************************************************************"
puts "xml_path: $xml_path , proc: $proc_1 ,  out_dir : $out_dir local repo path : $repo_path"

set err_code 0
set hw [hsi open_hw_design $xml_path]
hsi set_repo_path $repo_path
hsi create_sw_design sw1 -proc ${proc_1} -os device_tree
hsi set_property CONFIG.dt_overlay true [hsi get_os]
hsi generate_bsp -dir ${out_dir}
hsi close_hw_design $hw
exit $err_code
'

readarray -t SUBMIT_TXT_ARRAY <<<"${SUBMIT_TEMPLATE}"
declare -p SUBMIT_TXT_ARRAY

echo "$TEMP_FILE"
echo ""
printf '%s\n' "${SUBMIT_TXT_ARRAY[@]}"
if [ -f $TEMP_FILE ]
then
    rm -v $TEMP_FILE
fi
printf '%s\n' "${SUBMIT_TXT_ARRAY[@]}" > $TEMP_FILE
if [ ! -f $TEMP_FILE ]
then
    echo "$TEMP_FILE is not created"
    exit -1
fi

echo ""
echo "${EXE_FILE} ${TEMP_FILE} ${INPUT_FILE}  ps7_cortexa9_0 /work/vivado/device-tree-xlnx/ ${TEMP_DIR}"
${EXE_FILE} ${TEMP_FILE} ${INPUT_FILE}  ps7_cortexa9_0 /work/vivado/device-tree-xlnx/ ${TEMP_DIR}


FIRMWARE_BASE_NAME_OLD=$(sed -n "s|\(.*firmware-name = \"\)\(.\+\)\(\.bit\.bin\";.*\)|\2|p" ${TEMP_DIR}/pl.dtsi)
echo "firmware old name = ${FIRMWARE_BASE_NAME_OLD}"

if [ -f ${TEMP_DIR}/${FIRMWARE_BASE_NAME}.bit ]
then
    rm ${TEMP_DIR}/${FIRMWARE_BASE_NAME}.bit
fi


echo "cp ${TEMP_DIR}/${FIRMWARE_BASE_NAME_OLD}.bit ${TEMP_DIR}/${FIRMWARE_BASE_NAME}.bit"
cp ${TEMP_DIR}/${FIRMWARE_BASE_NAME_OLD}.bit ${TEMP_DIR}/${FIRMWARE_BASE_NAME}.bit


if [ -f ${TEMP_DIR}/${FIRMWARE_BASE_NAME}.bit.bin ]
then
    rm ${TEMP_DIR}/${FIRMWARE_BASE_NAME}.bit.bin
fi
${BASH_SOURCE_DIR}/bit2bin.sh -e /tools/Xilinx/Vivado/2019.2/bin/bootgen -i ${TEMP_DIR}/${FIRMWARE_BASE_NAME}.bit

sed "s|\(firmware-name = \"\)\(.\+\)\(\.bit\.bin\"\)|\1${FIRMWARE_BASE_NAME}\3|g" ${TEMP_DIR}/pl.dtsi > ${TEMP_DIR}/${FIRMWARE_BASE_NAME}.dtsi 
${BASH_SOURCE_DIR}/dtc -O dtb -o ${TEMP_DIR}/${FIRMWARE_BASE_NAME}.dtbo -b 0 -@ ${TEMP_DIR}/${FIRMWARE_BASE_NAME}.dtsi
