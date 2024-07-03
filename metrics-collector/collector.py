import psutil
import serial
from subprocess import PIPE, run
import time


DEVICE_NAME = "/dev/ttyACM0"
BAUD_RATE = 115200
CHECK_INTERVAL = 30


def sendSerialData(data, device, baudrate):
    dev = serial.Serial(
        port=device,
        baudrate=baudrate,
        timeout=1
    )

    dev.write(bytes(f"[{data}]", "utf-8"))


def checkHealth():
    # TODO
    result = 200

    return f"OVERALL:{result}"


def getCpuMetrics():
    cpuPercent = psutil.cpu_percent(interval=1)
    loadAvg = psutil.getloadavg()[2]
    
    return f"CPU:{cpuPercent},LAVG:{loadAvg}"


def getCpuTempMetrics():
    temp = psutil.sensors_temperatures()
    cpuTempRes = {}
    for elem in temp['coretemp']:
        if elem.label == "Package id 0":
            cpuTempRes[0] = elem.current
        if elem.label == "Package id 1":
            cpuTempRes[1] = elem.current

    output = "CPUTEMP:"
    for cpuid, cputemp in cpuTempRes.items():
        output += f"{cputemp}|"

    return output


def getDiskMetrics():
    slotNumIndex = 5
    queryDiskCmd = "find /dev/disk/by-path -type l -ls | grep -e scsi -e ata | grep -v sr | grep -v usb | grep -v part"
    diskInfo = run(queryDiskCmd, stdout=PIPE, stderr=PIPE, universal_newlines=True, shell=True).stdout.splitlines()

    identDisk = {}
    for di in diskInfo:
        if di is not None:
            if di.split(":")[slotNumIndex] is not None:
                identDisk[di.split("/")[-1]] = di.split(":")[slotNumIndex]
    
    diskUsage = {}
    pDiskStat = psutil.disk_io_counters(perdisk=True)
    time.sleep(1)
    diskStat = psutil.disk_io_counters(perdisk=True)
    for devName, slotNum in identDisk.items():

        querySmartCmd = f"smartctl --attributes --log=selftest --quietmode=errorsonly /dev/{devName}"
        smartOutput = run(querySmartCmd, stdout=PIPE, stderr=PIPE, universal_newlines=True, shell=True).stdout.splitlines()
        if smartOutput:
            smtResult = 2
        else:
            smtResult = 1

        pRead = pDiskStat[devName].read_count
        pWrite = pDiskStat[devName].write_count
        cRead = diskStat[devName].read_count
        cWrite = diskStat[devName].write_count
        diskUsage[slotNum] = (cRead - pRead, cWrite - pWrite, smtResult)

    output = "DISKIO:"
    for n, s in diskUsage.items():
        output += f"{n}-{s[0]}-{s[1]}-{s[2]}-|"

    return output


def getMemoryMetrics():
    memoryUsage = psutil.virtual_memory().percent

    return f"MEM:{memoryUsage}"


def getPowerUsage():
    pass


def main():
    payload = f",{getCpuMetrics()},{getMemoryMetrics()},{getDiskMetrics()},{getCpuTempMetrics()},{checkHealth()}\n"
    
    ## Uncomment to debug
    #print(getCpuMetrics())
    #print(getMemoryMetrics())
    #print(getDiskMetrics())
    #print(getCpuTempMetrics())
    #print(payload)
    
    sendSerialData(payload, DEVICE_NAME, BAUD_RATE)


if __name__ in "__main__":
    while(True):
        main()
        time.sleep(CHECK_INTERVAL)