import os, sys, subprocess, glob

if len(sys.argv) != 7:
	print("Usage: path to profiles, hypervisor, device name, fuzz bin path, output path, base fuzzer")
	sys.exit(1)

path = sys.argv[1]
hypervisor = sys.argv[2]
device = sys.argv[3]
target = sys.argv[4]
output = sys.argv[5]
fuzzer = sys.argv[6]

summary = path + 'coverage-reports/' + device + '/summary.txt'

try:
	# Change the current working directory
	os.chdir(path)
	print(f"Changed working directory to: {path}")

	# Verify the change
	current_workdir = os.getcwd()
	print(f"Current working directory: {current_workdir}")
	os.makedirs(path + 'coverage-reports', exist_ok=True)
	os.makedirs(path + 'coverage-reports/' + device, exist_ok=True)

except FileNotFoundError:
	print(f"Error: The directory {path} does not exist.")
except PermissionError:
	print(f"Error: You do not have permission to change to the directory {path}.")
except Exception as e:
	print(f"An unexpected error occurred: {e}")

# Get all profile

files = glob.glob("clangcovdump.profraw-*")

files = sorted(files)

output_f = open(output, 'w')

start_time = int(files[0].split('-')[1]) - 1

# Merge one each time

sources = []

if fuzzer == "videzzo" and hypervisor == "qemu":
	root = "/root/videzzo/videzzo_qemu/qemu/"
	llvm_prof = "llvm-profdata"
	llvm_cov = "llvm-cov"
elif hypervisor == "qemu":
	root = path + 'qemu/'
	llvm_prof = "llvm-profdata-n"
	llvm_cov = "llvm-cov-n"
elif hypervisor == "vbox":
	root = path + 'videzzo_vbox/vbox/'
	llvm_prof = "llvm-profdata-v"
	llvm_cov = "llvm-cov-v"

def get_full_path(path):
	return root + path

if hypervisor == "qemu":
	if device == "ac97":
		sources = [ "hw/audio/ac97.c", "audio/mixeng.c", "audio/audio.c", "hw/pci/pci.c"]
	elif device == "acpi-erst":
		sources = [ "hw/acpi/erst.c" ]
	elif device == "ahci-hd":
		sources = [ "hw/ide/ahci.c", "hw/ide/core.c" ]
	elif device == "am53c974":
		sources = [ "hw/scsi/esp-pci.c", "hw/scsi/esp.c", "hw/scsi/scsi-bus.c", "util/fifo8.c"]
	elif device == "ati":
		sources = [ "hw/display/ati.c", "hw/display/vga.c", "ui/console.c", "hw/display/ati_2d.c", "hw/display/ati_dbg.c", "hw/i2c/bitbang_i2c", "hw/pci/pci.c", "hw/i2c/core.c" ]
	elif device == "cirrus-vga":
		sources = [ "hw/display/cirrus_vga.c", "hw/display/vga.c"]
	elif device == "cs4231a":
		sources = [ "hw/audio/cs4231a.c", "audio/audio.c", "audio/mixeng.c"]
	elif device == "ctu-can":
		sources = [ "hw/net/can/ctucan_pci.c", "hw/net/can/ctucan_core.c", "net/can/can_core.c" ]
	elif device == "ehci":
		sources = [ "hw/usb/hcd-ehci-pci.c", "hw/usb/hcd-ehci.c", "hw/usb/core.c", "hw/usb/bus.c", "hw/usb/libhw.c" ]
	elif device == "es1370":
		sources = [ "hw/audio/es1370.c", "hw/pci/pci.c", "audio/audio.c", "audio/mixeng.c" ]
	elif device == "fdc":
		sources = [ "hw/block/fdc-sysbus.c", "hw/block/fdc.c" ]
	elif device == "imx-usb-phy":
		sources = [ "hw/usb/imx-usb-phy.c" ]
	elif device == "kvaser-can":
		sources = [ "hw/net/can/can_kvaser_pci.c", "hw/net/can/can_sja1000.c", "hw/pci/pci.c", "net/can/can_core.c" ]
	elif device == "lan9118":
		sources = [ "hw/net/lan9118.c", "hw/core/ptimer.c"]
	elif device == "lsi53c895a":
		sources = [ "hw/scsi/lsi53c895a.c", "hw/scsi/scsi-bus.c"]
	elif device == "megasas":
		sources = [ "hw/scsi/megasas.c", "hw/scsi/scsi-bus.c", "util/bitops.c"]
	elif device == "ne2000":
		sources = [ "hw/net/ne2000-pci.c", "hw/net/ne2000.c"]
	elif device == "nvme":
		sources = [ "hw/nvme/ctrl.c", "hw/nvme/ns.c", "hw/nvme/subsys.c", "hw/nvme/dif.c", "hw/pci/pcie_sriov.c"]
	elif device == "ohci":
		sources = [ "hw/usb/hcd-ohci-pci.c", "hw/usb/hcd-ohci.c", "hw/usb/core.c", "hw/usb/bus.c", "hw/usb/libhw.c" ]
	elif device == "parallel":
		sources = [ "hw/char/parallel.c" ]
	elif device == "pcm3680-can":
		sources = [ "hw/net/can/can_pcm3680_pci.c", "hw/net/can/can_sja1000.c", "net/can/can_core.c" ]
	elif device == "pcnet":
		sources = [ "hw/net/pcnet-pci.c", "hw/net/pcnet.c" ]
	elif device == "qxl":
		sources = [ "hw/display/qxl.c", "hw/display/vga.c", "ui/console.c", "hw/display/qxl-render.c", "hw/pci/pci.c" ]
	elif device == "rtl8139":
		sources = [ "hw/net/rtl8139.c" ]
	elif device == "sdhci":
		sources = [ "hw/sd/sdhci-pci.c", "hw/sd/sdhci.c", "hw/sd/core.c" ]
	elif device == "smc91c111":
		sources = [ "hw/net/smc91c111.c" ]
	elif device == "std-vga":
		sources = [ "hw/display/vga-mmio.c", "hw/display/vga.c" ]
	elif device == "stellaris-enet":
		sources = [ "hw/net/stellaris_enet.c" ]
	elif device == "tulip":
		sources = [ "hw/net/tulip.c", "hw/nvram/eeprom93xx.c" ]
	elif device == "uhci":
		sources = [ "hw/usb/hcd-uhci.c", "hw/usb/core.c", "hw/usb/bus.c", "hw/usb/libhw.c" ]
	elif device == "xgmac":
		sources = [ "hw/net/xgmac.c" ]
	else:
		print("unsupported device")
elif hypervisor == "vbox":
	if device == "ahci":
		sources = [ "src/VBox/Devices/Storage/DevAHCI.cpp" ]
	elif device == "ne2000":
		sources = [ "src/VBox/Devices/Network/DevDP8390.cpp" ]
	else:
		print("unsupported device")

for i in range(0, len(sources)):
	sources[i] = get_full_path(sources[i])

next_interval = 1

for f in files:
	# Generate the coverage summary
	print("Processing file", f)
	time = int(f.split('-')[1])
	interval = time - start_time
	if interval < next_interval and f != files[-1]:
		continue
	if next_interval < 60:
		next_interval = next_interval + 1
	elif next_interval < 600:
		next_interval = next_interval + 60
	elif next_interval < 3600:
		next_interval = next_interval + 600
	elif next_interval < 86400:
		next_interval = next_interval + 3600
	else:
		break
	command = [llvm_prof,'merge','-output=clangcovdump.profdata', f]
	try:
		result = subprocess.run(command, capture_output=True, text=True, check=True)
	except subprocess.CalledProcessError as e:
		print(f"Command failed with return code {e.returncode}")
		print(f"Error output:\n{e.stderr}")
	except FileNotFoundError:
		print("The command was not found.")
	except Exception as e:
		print(f"An unexpected error occurred: {e}")
	command = [llvm_cov, 'report', target, '-instr-profile', 'clangcovdump.profdata'] + sources
	with open(summary, 'w') as sumfile:
		try:
			result = subprocess.run(command, stdout=sumfile, stderr=subprocess.PIPE, text=True, check=True)
		except subprocess.CalledProcessError as e:
			print(f"Command failed with return code {e.returncode}")
			print(f"Error output:\n{e.stderr}")
		except FileNotFoundError:
			print("The command was not found.")
		except Exception as e:
			print(f"An unexpected error occurred: {e}")
	# Read the data
	with open(summary, 'r') as sumfile:
		lines = sumfile.readlines()
		branches = 0
		missed = 0
		# Process lines starting from the third line
		for line in lines[2:]:
			if line.startswith('-----'):
				break
			data = line.split()
			branches = branches + int(data[10])
			missed = missed + int(data[11])
			# Stop reading when encountering a line that starts with -----
		cov = round((1.0 - missed / branches) * 100, 2)
		# Append to the final summary
		output_f.write(str(interval))
		output_f.write(" ")
		output_f.write(str(cov))
		output_f.write("\n")


output_f.close()




