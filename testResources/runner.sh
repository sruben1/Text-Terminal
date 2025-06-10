
#!/bin/bash

make profiler

PROGRAM=./TestBuild.out

# Automatically increment counter each time script is called
run_number=1
while [ -d "./profilerRuns/run${run_number}" ]; do
    ((run_number++))
done

# Prepare directory:
# Create the directories if they don't exist
mkdir -p "./profilerRuns/run${run_number}"
mkdir -p ./testingText

# Create test files (https://stackoverflow.com/a/64585623):
echo "Starting Generating test files..." 
if [ ! -f "./testingText/smallFile.txt" ]; then
    tr -dc "A-Za-z 0-9" < /dev/urandom | fold -w75|head -n 200 > ./testingText/smallFile.txt 
fi
if [ ! -f "./testingText/mediumFile.txt" ]; then
tr -dc "A-Za-z 0-9" < /dev/urandom | fold -w200|head -n 100000 > ./testingText/mediumFile.txt
fi
if [ ! -f "./testingText/veryBigFile.txt" ]; then
tr -dc "A-Za-z 0-9" < /dev/urandom | fold -w207|head -n 10000000 > ./testingText/veryBigFile.txt
fi
echo "Ended Generating test files." 

#  >>>>>>>>>> Inserts Profiler <<<<<<<<<<
# Run 0 on 
for testIdx in {1..5}; do
echo "Running test ${testIdx}." 

files=("smallFile" "mediumFile" "veryBigFile")
if [[ $testIdx -eq 4 || $testIdx -eq 5 ]]; then
    files=("veryBigFile") # Only run veryBigFile for tests 4 and 5 (search)
fi

for file in "${files[@]}"; do

#Call our program:
${PROGRAM} "./testingText/${file}.txt" 0 ${testIdx}

# Check data exists first:
if [ -f "./profiler.log" ]; then
    mv ./profiler.log "./profilerRuns/run${run_number}/${testIdx}-Run-${file}.csv"
    
    echo "Profiler run completed. Log saved to ./profilerRuns/run${run_number}/${testIdx}-Run-${file}.csv"
else
    echo "Warning: profiler.log was not created, on test: ${testIdx} ${file}"
fi
#Small delay between program executions
sleep 1
done
done

# #  >>>>>>>>>> TEMPLATE <<<<<<<<<<
# for testIdx in {FROM..TO}; do
# echo "Running test ${testIdx}." 
# for file in "smallFile" "mediumFile" "veryBigFile"; do

# #Call our program:\mediumFile.txt
# ${PROGRAM} ""./testingText/${file}.txt" 0 ${testIdx}

# # Check data exists first:
# if [ -f "./profiler.log" ]; then
#     cp ./profiler.log "./profilerRuns/run${run_number}/${testIdx}-Run-${file}.csv"
    
#     echo "Profiler run completed. Log saved to ./profilerRuns/run${run_number}/${testIdx}-Run-${file}.csv"
# else
#     echo "Warning: profiler.log was not created, on test: ${testIdx} ${file}"
# fi
# #Small delay between program executions
# sleep 1
# done
# done