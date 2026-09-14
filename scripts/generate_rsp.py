import os, sys, re

if len(sys.argv) < 3:
	print("Please provide the arch and the fuzzer!")
	exit(1)

arch = sys.argv[1]
fuzzer = sys.argv[2]
core_file = ""
if len(sys.argv) == 4:
	core_file = sys.argv[3]

related_path = "./related-files.txt"
origin_rsp_path = "./qemu-" + fuzzer + "-" + arch + ".rsp"
new_rsp_path = "./qemu-" + fuzzer + "-new-" + arch + ".rsp"

def replace_ll_with_o(text):
	# Use a regular expression to find .ll at the end of the string
	return re.sub(r'\.ll$', '.o', text)


if os.path.isfile(related_path) and os.path.isfile(origin_rsp_path):
	with open(related_path, 'r') as related_f, open(origin_rsp_path, 'r') as origin_rsp_f:
		related_files = [replace_ll_with_o(line.rstrip()) for line in related_f.readlines()]
		link_cmd = origin_rsp_f.read()
		link_tokens = link_cmd.split(' ')
		new_tokens = []
		for token in link_tokens:
			if token == core_file or not token in related_files:
				new_tokens.append(token)
		with open(new_rsp_path, 'w') as new_rsp_f:
			for token in new_tokens:
				new_rsp_f.write(token)
				new_rsp_f.write(' ')
else:
	if not os.path.isfile(related_path):
		print("related files not found")
	if not os.path.isfile(origin_rsp_path):
		print("original rsp not found")
