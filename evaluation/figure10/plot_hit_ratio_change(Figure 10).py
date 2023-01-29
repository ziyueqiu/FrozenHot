import matplotlib.pyplot as plt
import mpl_toolkits.axisartist as axisartist

msr_fifo = [-0.07562, 0.546184, 0.439858, 0.307405, -0.06152, -0.00418, -0.45913, -0.43019, 0.063448, 0.72]
msr_lru = [-0.11075, 0.627922, 0.358341, 0.332037, -0.00776, 0.130577, -0.51198, -0.20913, -0.0063, 2.44361]
msr_name = ["prn_1", "proj_1", "proj_2", "proj_4", "", "", "src1_1", "usr_1", "", "web_2"]
msr_xbias = [0, 0, 0.02, 0, 0, 0, 0, 0, 0.05, -0.2]
msr_ybias = [-0.17, 0, 0.03, 0, 0, 0, 0, 0, 0, 0.02]
twitter_fifo = [-0.00295, 0.159558, 0.020645]
twitter_lru = [-0.03603, 0.170159, 0.01576]
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

plt.text(2.4, 0.76, "(244%, 351%)", fontsize=14, fontweight="semibold", fontstyle="italic")

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