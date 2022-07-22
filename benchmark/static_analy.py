import matplotlib.pyplot as plt
import sys

def static1(option):
    filelist = ["tr", "pt"]       
    for file in filelist:
        with open(file, "r", encoding='utf-8') as f:
            time = []
            x = []
            for line in f:
                line = line.strip('\n\t').split(' ')
                x.append(line[0])
                if option[0] == 1:
                    time.append(float(line[1]))
                elif option[0] == 2:
                    time.append(float(line[1]))
            plt.plot(x, time, label=file)
    plt.xlabel("malloc count",fontdict={'size': 13,'color':  'k'})
    plt.ylabel("time",fontdict={'size': 13,'color':  'k'})
    plt.tick_params(labelsize=12)
    ax = plt.gca()
    plt.tight_layout()
    ax.set_xticklabels(ax.get_xticklabels(), rotation=40, ha="right")
    plt.legend()
    if option[1] == 1:
        plt.title("malloc cost in 1 thread",fontdict={'size': 13,'color':  'k'})
        plt.savefig('./images/eval_1.png',bbox_inches='tight',dpi=300)
    elif option[1] == 2:
        plt.title("malloc cost in 20 thread",fontdict={'size': 13,'color':  'k'})
        plt.savefig('./images/eval_2_multithread.png',bbox_inches='tight',dpi=300)
    elif option[1] == 3:
        plt.title("malloc cost in 1 thread(random)",fontdict={'size': 13,'color':  'k'})
        plt.savefig('./images/eval_3_rand.png',bbox_inches='tight',dpi=300)
    elif option[1] == 4:
        plt.title("malloc cost in 10 thread(random)",fontdict={'size': 13,'color':  'k'})
        plt.savefig('./images/eval_4_rand_multi.png',bbox_inches='tight',dpi=300)

if __name__=="__main__":
    if len(sys.argv) < 2:
        print('usage: python static_analy.py [eval num] [other options]')
        sys.exit(-1)
    else:
        eval_num = int(sys.argv[1])

    if eval_num == 1:
        static1((1, 1, 1))
    if eval_num == 2:
        static1((2, 2, 20))
    if eval_num == 3:
        static1((1, 3, 1))
    if eval_num == 4:
        static1((1, 4, 10))


    