#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <asm/io.h>
#include <linux/circ_buf.h>


#define KBD_IRQ             1       /* IRQ number for keyboard (i8042) */
#define KBD_DATA_REG        0x60    /* I/O port for keyboard data */
#define KBD_SCANCODE_MASK   0x7f
#define KBD_STATUS_MASK     0x80
#define RELEASED	    1
#define PRESSED 	    0
#define DEVNAME	            "vkbdev"
#define SUCCESS            0
#define FAILED             -1
#define SET2_KEYMAP_SIZE   512
static int dev_id = 0;
static int inuse = 0;
//static int index = 0;
static unsigned short pvtbuf[4096];
static int head = 0;
static int tail = 0;
static int size = 4096;


static const unsigned short custom_keycode[SET2_KEYMAP_SIZE] = {

	  0, 67, 65, 63, 61, 59, 60, 88,  0, 68, 66, 64, 62, 15, 41,117,
	  0, 56, 42, 93, 29, 16,  2,  0,  0,  0, 44, 31, 30, 17,  3,  0,
	  0, 46, 45, 32, 18,  5,  4, 95,  0, 57, 47, 33, 20, 19,  6,183,
	  0, 49, 48, 35, 34, 21,  7,184,  0,  0, 50, 36, 22,  8,  9,185,
	  0, 51, 37, 23, 24, 11, 10,  0,  0, 52, 53, 38, 39, 25, 12,  0,
	  0, 89, 40,  0, 26, 13,  0,  0, 58, 54, 28, 27,  0, 43,  0, 85,
	  0, 86, 91, 90, 92,  0, 14, 94,  0, 79,124, 75, 71,121,  0,  0,
	 82, 83, 80, 76, 77, 72,  1, 69, 87, 78, 81, 74, 55, 73, 70, 99,

	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	217,100,255,  0, 97,165,  0,  0,156,  0,  0,  0,  0,  0,  0,125,
	173,114,  0,113,  0,  0,  0,126,128,  0,  0,140,  0,  0,  0,127,
	159,  0,115,  0,164,  0,  0,116,158,  0,172,166,  0,  0,  0,142,
	157,  0,  0,  0,  0,  0,  0,  0,155,  0, 98,  0,  0,163,  0,  0,
	226,  0,  0,  0,  0,  0,  0,  0,  0,255, 96,  0,  0,  0,143,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,107,  0,105,102,  0,  0,112,
	110,111,108,112,106,103,  0,119,  0,118,109,  0, 99,104,119,  0,

	  0,  0,  0, 65, 99,
};

static irqreturn_t custom_kbdroutine(int irq, void *dev_id)
{
        unsigned int scancode,temp;

	dump_stack();
        scancode = inb(KBD_DATA_REG);
        /* NOTE: i/o ops take a lot of time thus must be avoided in HW ISRs */
        if((scancode & KBD_STATUS_MASK) == PRESSED)
        {
//                pr_info("%s: Scan Code %x\n", __func__, scancode & KBD_SCANCODE_MASK);

		if (CIRC_SPACE(head, tail,size) >= 1)
		{
			temp = (scancode & 0x7f) | ((scancode & 0x80) << 1);
                	pvtbuf[head] = custom_keycode[temp];
			pr_info("%s: scancode = %x and pvtbuf[%d] = %d\n", __func__, scancode,head, pvtbuf[head]);
                	head = (head + 1) & (size-1);
		}
        }

        return IRQ_HANDLED;
}


static int open_chardev(struct inode *inode, struct file *file)
{
	if(inuse)
	{
		pr_err("%s: File is busy, unable to open\n",__func__);
		return -EBUSY;
	}
	inuse = 1;
	pr_info("%s: File opened successfully\n",__func__);

	if(request_irq(KBD_IRQ, custom_kbdroutine, IRQF_SHARED, "kbd2", &dev_id))
        {
                pr_err("%s: failed to register handler\n",__func__);
                return FAILED;
        }

	return SUCCESS;
}

static ssize_t write_chardev(struct file *file, const char __user *buf, size_t bufl, loff_t *offset)
{
	pr_info("%s: received data: [%s] with size %ld\n", __func__,buf,bufl);
	return bufl;
}

static ssize_t read_chardev(struct file *file, char __user *buf, size_t bufl, loff_t *offset)
{
	unsigned long ret;
//	pr_info("%s: reading keycode\n",__func__); 
	if ((head != tail) && (CIRC_CNT(head, tail, size) >= 1)) 
	{
		pr_info("%s: pvtbuf[%d] = %x\n",__func__,tail,pvtbuf[tail]); 
		ret = put_user(pvtbuf[tail],buf);
		if(ret < 0)
		{
			pr_err("%s: Failed to copy data\n",__func__);
			return ret;
		}
		tail = (tail+1) & (size-1);
	}
	return bufl;
}


static int release_chardev(struct inode *inode, struct file *file)
{
	inuse = 0;
	synchronize_irq(KBD_IRQ);
	free_irq(KBD_IRQ, &dev_id);

	pr_info("%s: file closed\n", __func__);
	return SUCCESS;
}

static struct file_operations chdev_ops = {
	.owner = THIS_MODULE,
	.open  = open_chardev,
 	.write = write_chardev,
	.read =  read_chardev,
	.release = release_chardev
};

static struct miscdevice vdevMisc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = DEVNAME,
	.mode = S_IRWXUGO,
	.fops = &chdev_ops
};

static int __init customkbd_init(void)
{
	int  ret = 0;

	ret = misc_register(&vdevMisc);
	if(ret < 0)
	{
		pr_err("%s: failed to register misc device\n",__func__);
		return FAILED;
	}

	pr_info("%s: keyboard custom module loaded\n",__func__);
	return 0;
}

static void __exit customkbd_exit(void)
{
	pr_info("%s: released handler\n",__func__);
	misc_deregister(&vdevMisc);
}

module_init(customkbd_init);
module_exit(customkbd_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Getting key value from keyboard");
MODULE_AUTHOR("Ashish Vara");
