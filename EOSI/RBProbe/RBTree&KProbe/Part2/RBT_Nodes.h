
#ifndef RBT_NODE_H
#define RBT_NODE_H

/* rb_object structure shared by instances
*  of RBT_530 module and RBprobe module
*/
typedef struct rb_object {
    int key;
    int data;
    struct rb_node node;
}rb_object_t;

/* struct rbt_dev
 * per device structure of RBT_530 module
*/ 

struct rbt_dev {
        rb_object_t last_node;//[instance] = RB_ROOT
	struct cdev cdev;               /* The cdev structure */
	char name[20];                  /* Name of device*/
        struct rb_root root_tree;//[instance] = RB_ROOT
};


#endif
