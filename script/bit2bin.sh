#! /usr/bin/env bash

THIS_CMD_BASENAME=$(basename $0)
ABOUT_THIS="\
convert from bit to .bit.bin
Usage: ${THIS_CMD_BASENAME} <-e bootgen_exe_file> <-i firmware_bit_file>

Example:
${THIS_CMD_BASENAME} -e /tools/Xilinx/Vivado/2019.2/bin/bootgen -i design_1_wrapper.bit
"

ORI_ARGS=( "$@" )

BIF_FILE=bit2bin.bif
FIRMWARE_BIT_FILE=base.bit
BOOTGEN_EXE=/tools/Xilinx/Vivado/2019.2/bin/bootgen

while getopts ":i:e:" opt; do
    case $opt in
        i)
	    FIRMWARE_BIT_FILE="$OPTARG"
	    ;;
        e)
	    BOOTGEN_EXE="$OPTARG"
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


# shift $(($OPTIND - 1))
# if [ "$#" -gt 0 ]
# then
#     TAIL_OPT_ARRAY=( "$@" )
# else
#     TAIL_OPT_ARRAY=()
# fi
# EXTRA_JOB_ARG=( "${TAIL_OPT_ARRAY[@]}" )
# if [ -z "$J_OPT" ]
# then
#     echo "Option: -j is required"
#     printf "$ABOUT_THIS"
#     exit 1
# fi

FIRMWARE_BIT_REAL_PATH=$(readlink -m $FIRMWARE_BIT_FILE)
SUBMIT_TEMPLATE="\
all:
{
  ${FIRMWARE_BIT_REAL_PATH}
}
"

readarray -t CONDOR_SUBMIT_TXT_ARRAY <<<"${SUBMIT_TEMPLATE}"
declare -p CONDOR_SUBMIT_TXT_ARRAY

echo "$BIF_FILE"
echo ""
printf '%s\n' "${CONDOR_SUBMIT_TXT_ARRAY[@]}"
if [ -f $BIF_FILE ]
then
    rm -v $BIF_FILE
fi
printf '%s\n' "${CONDOR_SUBMIT_TXT_ARRAY[@]}" > $BIF_FILE
if [ ! -f $BIF_FILE ]
then
    echo "$BIF_FILE is not created"
    exit -1
fi

FIRMWARE_DIR=$(dirname ${FIRMWARE_BIT_REAL_PATH})
FIRMWARE_BASE_NAME=$(basename ${FIRMWARE_BIT_REAL_PATH} .bit)

FIRMWARE_BIN_EXPECTED=$(readlink -m ${FIRMWARE_DIR}/${FIRMWARE_BASE_NAME}.bit.bin)
if [ -f ${FIRMWARE_BIN_EXPECTED} ]
then
    rm -v ${FIRMWARE_BIN_EXPECTED}
fi

echo ""
echo "${BOOTGEN_EXE} -image $BIF_FILE -arch zynq -w -process_bitstream bin"
${BOOTGEN_EXE} -image $BIF_FILE -arch zynq -w -process_bitstream bin
if [ -f $BIF_FILE ]
then
    rm -v $BIF_FILE
fi

if [ -f ${FIRMWARE_BIN_EXPECTED} ]
then
    echo ""
    echo "create firmware bit.bin"
    echo "${FIRMWARE_BIN_EXPECTED}"
else
    echo ""
    echo "error"
fi
   
