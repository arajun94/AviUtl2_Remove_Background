"""
https://matplotlib.org/users/event_handling.html
"""

from matplotlib import pyplot as plt
from matplotlib import widgets as wg
from matplotlib.colors import ListedColormap
from matplotlib.gridspec import GridSpec
from PIL import Image
import japanize_matplotlib
import numpy as np

LEFT_CLICK = 1
RIGHT_CLICK = 3

plt.rcParams["figure.dpi"] = 100

cmaprb = ListedColormap(['red', 'blue'])

dpi= 150


def update(event_handler):
    def event_handler_decorated(self, *args, **kwargs):
        event_handler(self, *args, **kwargs)
        # self.plot_objects.set_data(self.points[:,0], self.points[:,1], self.points[:,2])
        self.sc.set_offsets(self.points[:,0:2])
        self.sc.set_array(self.points[:,2])
        self.fig.canvas.draw()
    return event_handler_decorated


class PointHandler:

    def __init__(self, fig, ax, points):
        self.fig = fig
        self.ax = ax

        self.undo_stack = []

        # self.plot_objects, = ax.plot(self.points[:,0], self.points[:,1], 'bo', picker=5)
        self.sc = self.ax.scatter(self.points[:,0], self.points[:,1], c=self.points[:,2], edgecolor='white', linewidth=1.25, cmap=cmaprb, picker=5, vmin=0, vmax=1)

    @update
    def on_pressed(self, event):
        if event.button != LEFT_CLICK:
            return
        if event.inaxes != self.ax:
            return
        
        if event.key == 'shift':
            self.points = np.append(self.points, [[event.xdata, event.ydata, 0]], axis=0)
        else:
            self.points = np.append(self.points, [[event.xdata, event.ydata, 1]], axis=0)
    
    @update
    def key_event(self, event):
        if event.key == 'ctrl+z' and len(self.points) >0:
            self.undo_stack.append(self.points[-1,:])
            self.points = np.delete(self.points, -1, axis=0)

        if event.key == 'ctrl+y' and len(self.undo_stack) > 0:
            self.points = np.append(self.points, [self.undo_stack[-1]], axis=0)
            self.undo_stack = self.undo_stack[:-1]

    @update
    def on_picked(self, event):
        self.select_index = event.ind[0]
        
        if event.mouseevent.button == RIGHT_CLICK:
            self.points = np.delete(self.points, self.select_index, axis=0)


def show_mask(mask, ax, obj_id=None, random_color=False):
    if random_color:
        color = np.concatenate([np.random.random(3), np.array([0.6])], axis=0)
    else:
        cmap = plt.get_cmap("tab10")
        cmap_idx = 0 if obj_id is None else obj_id
        color = np.array([*cmap(cmap_idx)[:3], 0.6])
    h, w = mask.shape[-2:]
    mask_image = mask.reshape(h, w, 1) * color.reshape(1, 1, -1)
    ax.imshow(mask_image)


def show_points(coords, labels, ax):
    ax.scatter(coords[:,0], coords[:,1], c=labels, edgecolor='white',linewidth=1.25, cmap=cmaprb, picker=5, vmin=0, vmax=1)

def previewer(image_path, masks_logits, obj_ids, points, labels):
    fig = plt.figure(dpi=dpi)
    gs = GridSpec(2, 2, figure=fig, width_ratios=[1, 1], height_ratios=[8, 1])
    ax_main = fig.add_subplot(gs[0, :])
    ax_btn1 = fig.add_subplot(gs[1, 0])
    ax_btn2 = fig.add_subplot(gs[1, 1])
    ax_main.set_title("プレビュー\nYESで続行、NOで再プロット")
    ax_main.imshow(Image.open(image_path))

    ok = False

    def btn1_callback(event):
        nonlocal ok
        ok = False
        plt.close(fig)

    def btn2_callback(event):
        nonlocal ok
        ok = True
        plt.close(fig)

    btn1 = wg.Button(ax_btn1, 'NO', color='lightgoldenrodyellow', hovercolor='0.98')
    btn1.on_clicked(btn1_callback)
    btn2 = wg.Button(ax_btn2, 'YES', color='lightgoldenrodyellow', hovercolor='0.98')
    btn2.on_clicked(btn2_callback)

    show_points(points, labels, ax_main)
    for i, obj_id in enumerate(obj_ids):
        # show_points(*prompts[out_obj_id], plt.gca())
        show_mask((masks_logits[i] > 0.0).cpu().numpy(), ax_main, obj_id=obj_id)
    plt.show()

    return ok

def plotter(image_path, points=np.empty((0,3))):
    fig, ax = plt.subplots(2,1, gridspec_kw=dict(width_ratios=[1], height_ratios=[8, 1]), dpi=dpi)
    ax[0].set_title("左クリックで対象物体を選択\nShift+左クリックで非対象物体を選択\n右クリックで点削除")
    pthandler = PointHandler(fig, ax[0], points)

    fig.canvas.mpl_connect("button_press_event", pthandler.on_pressed)
    fig.canvas.mpl_connect("key_press_event", pthandler.key_event)
    fig.canvas.mpl_connect("pick_event", pthandler.on_picked)

    btn1 = wg.Button(ax[1], 'end', color='lightgoldenrodyellow', hovercolor='0.98')
    btn1.on_clicked(lambda event:plt.close(fig))

    ax[0].imshow(np.array(Image.open(image_path)))
    plt.show()
    return pthandler.points