TRACES_DIR="/home/shihao-song/Documents/Research_Extend_1/New_Traces/CASES_2019"
CONFIGS_DIR="/home/shihao-song/Documents/Research/Tools_Development/Hybrid-eDRAM-PCM-Simulator/Configs/cfg_files"

TRACES=$(ls $TRACES_DIR)
CONFIGS=$(ls $CONFIGS_DIR | grep $1)

for trace in $TRACES
do
    full_trace_name=$TRACES_DIR"/"$trace
    for config in $CONFIGS
    do
        full_config_name=$CONFIGS_DIR"/"$config
        ./PCMSim $full_config_name $full_trace_name
	echo "*********************************************************"
    done
done
