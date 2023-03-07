import matplotlib.pyplot as plt
import matplotlib as mpl
import mpl_toolkits.axisartist as axisartist

mpl.rcParams['pdf.fonttype'] = 42
mpl.rcParams['ps.fonttype'] = 42 

msr_fifo = [-0.1309, -0.13834, 0.617683, 0.381737, 0.306024, -0.0583, 0.011611, -0.5111, -0.16167, 0.08462, 0.72]
msr_lru = [-0.1061, -0.4712, 0.705716, 0.49599, 0.3078, -0.07968, 0.084431, -0.44263, -0.01759, 0.06967, 2.516114]
msr_name = ["", "prn_1", "proj_1", "proj_2", "proj_4", "", "", "src1_1", "", "", "web_2"]
msr_xbias = [0, 0, 0.02, 0, 0, 0, 0, 0, 0.05, -0.2, -0.14]
msr_ybias = [-0.17, -0.17, 0.03, 0, 0, 0, 0, 0, 0, 0.02, 0.0]
twitter_fifo = [-0.00342, 0.190441, 0.039459]
twitter_lru = [-0.05771, 0.300502, 0.026519]
twitter_name= ["", "T45", ""]
twitter_xbias = [-0.02, 0.14, 0, ]
twitter_ybias = [0.04, -0.08, 0, ]

fig = plt.figure(figsize=(9.5,3.6))

ax = axisartist.Subplot(fig, 111)
fig.add_axes(ax)
ax.axis[:].set_visible(False)
ax.axis["x"] = ax.new_floating_axis(0,0)
ax.axis["y"] = ax.new_floating_axis(1,0)
ax.axis["x"].set_axisline_style("->", size = 1.5)
ax.axis["y"].set_axisline_style("->", size = 1.5)
ax.axis['x'].major_ticklabels.set_fontsize(13)
ax.axis['y'].major_ticklabels.set_fontsize(13)

for x,y,name,xbias,ybias in zip(msr_lru, msr_fifo, msr_name, msr_xbias, msr_ybias):
    plt.text(x + xbias, y + 0.02 + ybias, name, ha='center', va='bottom', fontsize=14)
for x,y,name,xbias,ybias in zip(twitter_lru, twitter_fifo, twitter_name, twitter_xbias, twitter_ybias):
    plt.text(x + xbias, y+0.02 + ybias, name, ha='center', va='bottom', fontsize=14)

plt.text(2.51, 0.76, "(298%, 318%)", fontsize=14, fontweight="semibold", fontstyle="italic")

plt.text(2.55, 0.05, "LRU (%)", fontweight="semibold", fontsize=13)
plt.text(0.10, 0.8, "FIFO (%)", fontweight="semibold", fontsize=13)
plt.scatter(x=msr_lru,y=msr_fifo,c="green",s=100, marker="^", label="MSR")
plt.scatter(x=twitter_lru,y=twitter_fifo,c="red",s=100, marker="o", label="Twitter")

plt.xlim(xmin = -0.55, xmax = 2.6)
# plt.xticks([-0.5, -0.25, 0.25, 0.5, 0.75, 1.0, 1.25, 1.5], ["-50", "-25", "25", "50", "75", "100", "125", "150"], fontsize=20)
plt.xticks([-0.5, -0.25, 0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 1.75, 2.00, 2.25, 2.5], ["-50", "-25", "25", "50", "75", "100", "125", "150", "175", "200", "225", "250"], fontsize=20)
plt.xticks([-0.5, 0.5, 1.0, 1.5, 2.00, 2.5], ["-50", "50", "100", "150", "200", "250"], fontsize=20)


plt.ylim(ymin = -0.55, ymax = 0.8)
# plt.yticks([-0.5, -0.25, 0.25, 0.5, 0.75, 1.0], ["-50", "-25", "25", "50", "75", "100"], fontweight="black")
plt.yticks([-0.5, 0.5,], ["-50", "50",], fontweight="black")

plt.axis('on')

plt.legend(loc='upper left', frameon=False, fontsize=13, handletextpad=0.20)

# plt.show()
plt.savefig("Hit-ratio-change.pdf", bbox_inches='tight')