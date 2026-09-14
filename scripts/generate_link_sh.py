import os, sys, re

if len(sys.argv) < 3:
	print("Please provide the command file and output path!")
	exit(1)

cmd_path = sys.argv[1]
output_path = sys.argv[2]
core_file = ""
if len(sys.argv) == 4:
	core_file = sys.argv[3]

related_path = "./related-files.txt"

def replace_ll_with_o(text):
	# Use a regular expression to find .ll at the end of the string
	return re.sub(r'\.ll$', '.o', text)


if os.path.isfile(related_path) and os.path.isfile(cmd_path):
	with open(related_path, 'r') as related_f, open(cmd_path, 'r') as cmd_f:
		related_files = [replace_ll_with_o(line.rstrip()) for line in related_f.readlines()]
		lines = cmd_f.readlines()
		link_cmd = lines[-1].split(']', 1)[-1].lstrip()
		link_tokens = link_cmd.split(' ')
		new_tokens = []
		for token in link_tokens:
			if token == core_file or not token in related_files:
				new_tokens.append(token)
		with open(output_path, 'w') as output_f:
			output_f.write("#!/bin/bash\n")
			for token in new_tokens:
				output_f.write(token)
				output_f.write(' ')
