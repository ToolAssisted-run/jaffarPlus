#!/bin/bash

scriptFiles=`find ../examples -path ../examples/archive -prune -o -name "*.jaffar" -print`

set -e

# Getting absolute path to the Jaffar engine
jaffarPath=`realpath ../build/jaffar`

# Override values
export JAFFAR_IS_DRY_RUN=1
export JAFFAR_DRIVER_OVERRIDE_DRIVER_MAX_STEP=1
export JAFFAR_ENGINE_OVERRIDE_MAX_STATEDB_SIZE_MB=10
export JAFFAR_ENGINE_OVERRIDE_MAX_THREAD_COUNT=1

# Getting emulator to test
emulatorLine='"Emulator Name":'
emulatorName=${1}

for scriptFile in ${scriptFiles}
do
 dirName=`dirname ${scriptFile}`
 fileName=`basename ${scriptFile}`
 
 # Checking if the selected emulator is being tested
 set +e
 cat ${scriptFile} | grep "${emulatorLine}" > /dev/null
 emulatorLineFound=$?
 cat ${scriptFile} | grep \"${emulatorName}\" > /dev/null
 emulatorNameFound=$?
 set -e

 if [ ${emulatorLineFound} -ne 0 ]; then
   echo "File: ${scriptFile} has no ${emulatorLine} entry"
   exit -1
 fi

 if [ ${emulatorNameFound} -eq 0 ]; then
   command="${jaffarPath} --dryRun ${scriptFile}"
   echo ${command}
   ${command}
 fi
done
