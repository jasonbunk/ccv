#!/usr/bin/python
import sys, os
sysargs = sys.argv[1:]

# -----------------------------------------
# draws the results of cnn classification
# over the image, listing the guesses with confidence values


if len(sysargs) < 3:
	quit()

words = open(sysargs[0],"r+")
labels = {}
i = 1

for line in words:
	word = line.split(',')
	labels[str(i)] = word[0]
	i = i + 1


draw = ""
y = 15
#STDIN.each_line do |line|
while True:
	line = raw_input("input: ")
	print line
	args = line.split(' ')
	if args[0] == 'elapsed':
		break
	for jj in range(len(args) / 2):
		draw += "-fill none -strokewidth 1 -stroke DodgerBlue -draw \"rectangle 15,%d,165,%d\" -fill DodgerBlue -draw \"rectangle 15,%d,%d,%d\" -strokewidth 0 -stroke none -fill red -draw 'text 18,%d \"%s\"' " % (y, y + 16, y, (int(float(args[jj*2+1])*150)) + 15, y + 16, y + 13, labels[args[jj * 2]])
		y += 31

execstr = "convert %s %s%s" % (sysargs[1], draw, sysargs[2])
print("executing \""+str(execstr)+"\"")
os.system(execstr)
