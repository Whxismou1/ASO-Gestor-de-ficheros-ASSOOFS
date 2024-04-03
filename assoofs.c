//@ts-check
#include <linux/module.h>      /* Needed by all modules */
#include <linux/kernel.h>      /* Needed for KERN_INFO  */
#include <linux/init.h>        /* Needed for the macros */
#include <linux/fs.h>          /* libfs stuff           */
#include <linux/buffer_head.h> /* buffer_head           */
#include <linux/slab.h>        /* kmem_cache            */
#include "assoofs.h"
/*
    Mis funciones
*/
struct assoofs_inode_info *assoofs_get_inode_info(struct super_block *sb, uint64_t inode_no);
static struct inode *assoofs_get_inode(struct super_block *sb, int ino);
int assoofs_save_inode_info(struct super_block *sb, struct assoofs_inode_info *inode_info);

/*
 *  Operaciones sobre ficheros
 */
ssize_t assoofs_read(struct file *filp, char __user *buf, size_t len, loff_t *ppos);
ssize_t assoofs_write(struct file *filp, const char __user *buf, size_t len, loff_t *ppos);
const struct file_operations assoofs_file_operations = {
    .read = assoofs_read,
    .write = assoofs_write,
};

ssize_t assoofs_read(struct file *filp, char __user *buf, size_t len, loff_t *ppos)
{
    struct assoofs_inode_info *inode_info;
    struct buffer_head *bh;
    int nBytes;
    char *buffer;

    printk(KERN_INFO "Read request\n");
    inode_info = filp->f_path.dentry->d_inode->i_private;
    if (*ppos >= inode_info->file_size)
    {
        return 0;
    }

    bh = sb_bread(filp->f_path.dentry->d_inode->i_sb, inode_info->data_block_number);
    buffer = (char *)bh->b_data;

    buffer += *ppos;

    nBytes = min((size_t)inode_info->file_size - (size_t)*ppos, len);

    if (copy_to_user(buf, buffer, nBytes) != 0)
    {
        brelse(bh);
        return -EFAULT;
    }

    *ppos += nBytes;

    brelse(bh);
    return nBytes;
}

ssize_t assoofs_write(struct file *filp, const char __user *buf, size_t len, loff_t *ppos)
{
    struct buffer_head *bh;
    struct assoofs_inode_info *inode_info = filp->f_path.dentry->d_inode->i_private;
    char *buffer;
    int ret;
    struct super_block *sb;

    printk(KERN_INFO "Write request\n");
    if (*ppos + len > ASSOOFS_DEFAULT_BLOCK_SIZE)
    {
        printk(KERN_ERR "Write request exceeds the size of the block\n");
        return -ENOSPC;
    }

    buffer = (char *)bh->b_data;
    buffer += *ppos;
    ret = copy_from_user(buffer, buf, len);

    if (ret != 0)
    {
        return -EFAULT;
    }

    *ppos += len;
    mark_buffer_dirty(bh);
    sync_dirty_buffer(bh);

    inode_info->file_size = *ppos;
    sb = filp->f_path.dentry->d_inode->i_sb;
    assoofs_save_inode_info(sb, inode_info);

    return len;
}

/*
 *  Operaciones sobre directorios
 */
static int assoofs_iterate(struct file *filp, struct dir_context *ctx);
const struct file_operations assoofs_dir_operations = {
    .owner = THIS_MODULE,
    .iterate = assoofs_iterate,
};

static int assoofs_iterate(struct file *filp, struct dir_context *ctx)
{

    struct inode *inode;
    struct super_block *sb;
    struct assoofs_inode_info *inode_info;
    struct buffer_head *bh;
    struct assoofs_dir_record_entry *record;
    int i;

    printk(KERN_INFO "Iterate request\n");

    inode = filp->f_path.dentry->d_inode;
    sb = inode->i_sb;
    inode_info = inode->i_private;

    if (ctx->pos)
    {
        return 0;
    }

    if ((!S_ISDIR(inode_info->mode)))
    {
        return -1;
    }

    bh = sb_bread(sb, inode_info->data_block_number);
    record = (struct assoofs_dir_record_entry *)bh->b_data;
    for (i = 0; i < inode_info->dir_children_count; i++)
    {
        dir_emit(ctx, record->filename, ASSOOFS_FILENAME_MAXLEN, record->inode_no, DT_UNKNOWN);
        ctx->pos += sizeof(struct assoofs_dir_record_entry);
        record++;
    }

    brelse(bh);

    return 0;
}

/*
 *  Operaciones sobre inodos
 */
static int assoofs_create(struct user_namespace *mnt_userns, struct inode *dir, struct dentry *dentry, umode_t mode, bool excl);
struct dentry *assoofs_lookup(struct inode *parent_inode, struct dentry *child_dentry, unsigned int flags);
static int assoofs_mkdir(struct user_namespace *mnt_userns, struct inode *dir, struct dentry *dentry, umode_t mode);
static struct inode_operations assoofs_inode_ops = {
    .create = assoofs_create,
    .lookup = assoofs_lookup,
    .mkdir = assoofs_mkdir,
};

static struct inode *assoofs_get_inode(struct super_block *sb, int ino)
{
    // PAso 1
    struct inode *inode = new_inode(sb);
    struct assoofs_inode_info *inode_info = assoofs_get_inode_info(sb, ino);
    // PASO 2
    if (S_ISDIR(inode_info->mode))
    {
        inode->i_ino = ino;
        inode->i_sb = sb;
        inode->i_op = &assoofs_inode_ops;
        inode->i_fop = &assoofs_dir_operations;
        inode->i_atime = inode->i_mtime = inode->i_ctime = current_time(inode);
        inode->i_private = inode_info;
    }
    else if (S_ISREG(inode_info->mode))
    {
        inode->i_ino = ino;
        inode->i_sb = sb;
        inode->i_op = &assoofs_inode_ops;
        inode->i_fop = &assoofs_file_operations;
        inode->i_atime = inode->i_mtime = inode->i_ctime = current_time(inode);
        inode->i_private = inode_info;
    }
    else
    {
        printk(KERN_ERR "Unknown inode type. Neither a directory nor a file\n");
    }

    // PASO 3
    return inode;
}

struct dentry *assoofs_lookup(struct inode *parent_inode, struct dentry *child_dentry, unsigned int flags)
{

    struct assoofs_inode_info *parent_info;
    struct super_block *sb;
    struct buffer_head *bh;
    struct assoofs_dir_record_entry *record;
    int i;

    printk(KERN_INFO "Lookup request\n");

    parent_info = parent_inode->i_private;
    sb = parent_inode->i_sb;
    bh = sb_bread(sb, parent_info->data_block_number);

    record = (struct assoofs_dir_record_entry *)bh->b_data;
    for (i = 0; i < parent_info->dir_children_count; i++)
    {
        if (!strcmp(record->filename, child_dentry->d_name.name))
        {
            struct inode *inode = assoofs_get_inode(sb, record->inode_no);
            inode_init_owner(sb->s_user_ns, inode, parent_inode, ((struct assoofs_inode_info *)inode->i_private)->mode);
            d_add(child_dentry, inode);
            return NULL;
        }
        record++;
    }

    return NULL;
}
int assoofs_sb_get_a_freeblock(struct super_block *sb, uint64_t *block);
void assoofs_save_sb_info(struct super_block *vsb);
void assoofs_save_sb_info(struct super_block *vsb)
{
    struct buffer_head *bh;
    struct assoofs_super_block_info *sb = vsb->s_fs_info;
    bh = sb_bread(vsb, ASSOOFS_SUPERBLOCK_BLOCK_NUMBER);
    bh->b_data = (char *)sb;

    mark_buffer_dirty(bh);
    sync_dirty_buffer(bh);
    brelse(bh);
}
int assoofs_sb_get_a_freeblock(struct super_block *sb, uint64_t *block)
{
    struct assoofs_super_block_info *afs_sb = sb->s_fs_info;
    int i;
    for (i = 2; i < ASSOOFS_MAX_FILESYSTEM_OBJECTS_SUPPORTED; i++)
    {
        if (afs_sb->free_blocks & (1 << i))
        {
            break;
        }
    }
    if (i == ASSOOFS_MAX_FILESYSTEM_OBJECTS_SUPPORTED)
    {
        printk(KERN_ERR "No free blocks available\n");
        return -1;
    }
    *block = i;

    afs_sb->free_blocks &= ~(1 << i);
    assoofs_save_sb_info(sb);

    return 0;
}

void assoofs_add_inode_info(struct super_block *sb, struct assoofs_inode_info *inode);
void assoofs_add_inode_info(struct super_block *sb, struct assoofs_inode_info *inode)
{
    struct buffer_head *bh;
    struct assoofs_super_block_info *assoofs_sb = sb->s_fs_info;
    struct assoofs_inode_info *inode_info;

    bh = sb_bread(sb, ASSOOFS_INODESTORE_BLOCK_NUMBER);
    inode_info = (struct assoofs_inode_info *)bh->b_data;
    inode_info += assoofs_sb->inodes_count;
    memcpy(inode_info, inode, sizeof(struct assoofs_inode_info));

    mark_buffer_dirty(bh);
    sync_dirty_buffer(bh);
    assoofs_sb->inodes_count++;
    assoofs_save_sb_info(sb);
}

struct assoofs_inode_info *assoofs_search_inode_info(struct super_block *sb, struct assoofs_inode_info *start, struct assoofs_inode_info *search);

struct assoofs_inode_info *assoofs_search_inode_info(struct super_block *sb, struct assoofs_inode_info *start, struct assoofs_inode_info *search)
{
    uint64_t count = 0;

    while (start->inode_no != search->inode_no && count < ((struct assoofs_super_block_info *)sb->s_fs_info)->inodes_count)
    {
        count++;
        start++;
    }

    if (start->inode_no == search->inode_no)
    {
        return start;
    }
    else
    {
        return NULL;
    }
}

int assoofs_save_inode_info(struct super_block *sb, struct assoofs_inode_info *inode_info)
{
    struct buffer_head *bh;
    struct assoofs_inode_info *inode_pos;
    bh = sb_bread(sb, ASSOOFS_INODESTORE_BLOCK_NUMBER);
    inode_pos = assoofs_search_inode_info(sb, (struct assoofs_inode_info *)bh->b_data, inode_info);

    if (inode_pos == NULL)
    {
        printk(KERN_ERR "The inode to be saved does not exist\n");
        return -1;
    }

    memcpy(inode_pos, inode_info, sizeof(*inode_pos));
    mark_buffer_dirty(bh);
    sync_dirty_buffer(bh);

    return 0;
}

static int assoofs_create(struct user_namespace *mnt_userns, struct inode *dir, struct dentry *dentry, umode_t mode, bool excl)
{
    struct inode *inode;
    uint64_t count;
    struct super_block *sb;
    struct assoofs_inode_info *inode_info;
    struct assoofs_inode_info *parent_inode_info;
    struct assoofs_dir_record_entry *dir_contents;
    struct buffer_head *bh;

    printk(KERN_INFO "New file request\n");

    sb = dir->i_sb;

    count = ((struct assoofs_super_block_info *)sb->s_fs_info)->inodes_count;
    inode = new_inode(sb);
    inode->i_sb = sb;
    inode->i_atime = inode->i_mtime = inode->i_ctime = current_time(inode);
    inode->i_op = &assoofs_inode_ops;
    inode->i_ino = count + 1;

    if (count >= ASSOOFS_MAX_FILESYSTEM_OBJECTS_SUPPORTED)
    {
        printk(KERN_ERR "assoofs_create: max number of objects reached\n");
        return -1;
    }

    inode_info = kmalloc(sizeof(struct assoofs_inode_info), GFP_KERNEL);
    inode_info->inode_no = inode->i_ino;
    inode_info->mode = mode;
    inode_info->file_size = 0;
    inode->i_private = inode_info;

    inode->i_fop = &assoofs_file_operations;
    inode_init_owner(sb->s_user_ns, inode, dir, mode);
    d_add(dentry, inode);

    assoofs_sb_get_a_freeblock(sb, &inode_info->data_block_number);

    assoofs_add_inode_info(sb, inode_info);

    // PASO 2

    parent_inode_info = dir->i_private;
    bh = sb_bread(sb, parent_inode_info->data_block_number);

    dir_contents = (struct assoofs_dir_record_entry *)bh->b_data;
    dir_contents += parent_inode_info->dir_children_count;
    dir_contents->inode_no = inode_info->inode_no;

    strcpy(dir_contents->filename, dentry->d_name.name);
    mark_buffer_dirty(bh);
    sync_dirty_buffer(bh);
    brelse(bh);

    // PASO 3
    parent_inode_info->dir_children_count++;
    assoofs_save_inode_info(sb, parent_inode_info);

    // PASO 4
    return 0;
}

static int assoofs_mkdir(struct user_namespace *mnt_userns, struct inode *dir, struct dentry *dentry, umode_t mode)
{

    struct buffer_head *bh;
    struct inode *inode;
    uint64_t count;
    struct super_block *sb;
    struct assoofs_inode_info *inode_info;
    struct assoofs_inode_info *parent_inode_info;
    struct assoofs_dir_record_entry *dir_contents;

    printk(KERN_INFO "New directory request\n");

    sb = dir->i_sb;
    count = ((struct assoofs_super_block_info *)sb->s_fs_info)->inodes_count;
    inode = new_inode(sb);
    inode->i_sb = sb;
    inode->i_atime = inode->i_mtime = inode->i_ctime = current_time(inode);
    inode->i_op = &assoofs_inode_ops;
    inode->i_ino = count + 1;

    if (count >= ASSOOFS_MAX_FILESYSTEM_OBJECTS_SUPPORTED)
    {
        printk(KERN_ERR "assoofs_create: max number of objects reached\n");
        return -1;
    }

    inode_info = kmalloc(sizeof(struct assoofs_inode_info), GFP_KERNEL);
    inode_info->inode_no = inode->i_ino;
    inode_info->mode = S_IFDIR | mode;
    inode_info->dir_children_count = 0;
    inode_info->file_size = 0;
    inode->i_private = inode_info;

    inode->i_fop = &assoofs_file_operations;
    inode_init_owner(sb->s_user_ns, inode, dir, inode_info->mode);
    d_add(dentry, inode);

    assoofs_sb_get_a_freeblock(sb, &inode_info->data_block_number);

    assoofs_add_inode_info(sb, inode_info);

    // PASO 2

    parent_inode_info = dir->i_private;
    bh = sb_bread(sb, parent_inode_info->data_block_number);

    dir_contents = (struct assoofs_dir_record_entry *)bh->b_data;
    dir_contents += parent_inode_info->dir_children_count;
    dir_contents->inode_no = inode_info->inode_no;

    strcpy(dir_contents->filename, dentry->d_name.name);
    mark_buffer_dirty(bh);
    sync_dirty_buffer(bh);
    brelse(bh);

    // PASO 3
    parent_inode_info->dir_children_count++;
    assoofs_save_inode_info(sb, parent_inode_info);

    // PASO 4
    return 0;
}

/*
 *  Operaciones sobre el superbloque
 */
static const struct super_operations assoofs_sops = {
    .drop_inode = generic_delete_inode,
};

/*
 *  Inicialización del superbloque
 */
int assoofs_fill_super(struct super_block *sb, void *data, int silent)
{
    // 1.- Leer la información persistente del superbloque del dispositivo de bloques
    struct assoofs_super_block_info *assoofs_sb;
    struct buffer_head *bh;
    struct inode *root_inode;

    printk(KERN_INFO "assoofs_fill_super request\n");

    bh = sb_bread(sb, ASSOOFS_SUPERBLOCK_BLOCK_NUMBER);

    assoofs_sb = (struct assoofs_super_block_info *)bh->b_data;
    // 2.- Comprobar los parámetros del superbloque
    if (ASSOOFS_MAGIC != assoofs_sb->magic || ASSOOFS_DEFAULT_BLOCK_SIZE != assoofs_sb->block_size)
    {
        printk(KERN_ERR "assoofs_fill_super: wrong magic number or block size\n");
        return -1;
    }

    // 3.- Escribir la información persistente leída del dispositivo de bloques en el superbloque sb, incluído el campo
    // s_op con las operaciones que soporta.
    sb->s_magic = ASSOOFS_MAGIC;
    sb->s_maxbytes = ASSOOFS_DEFAULT_BLOCK_SIZE;
    sb->s_op = &assoofs_sops;
    sb->s_fs_info = assoofs_sb;
    // 4.- Crear el inodo raíz y asignarle operaciones sobre inodos (i_op) y sobre directorios (i_fop)
    root_inode = new_inode(sb);
    inode_init_owner(sb->s_user_ns, root_inode, NULL, S_IFDIR);
    root_inode->i_ino = ASSOOFS_ROOTDIR_INODE_NUMBER;
    root_inode->i_sb = sb;
    root_inode->i_op = &assoofs_inode_ops;
    root_inode->i_fop = &assoofs_dir_operations;
    root_inode->i_atime = root_inode->i_mtime = root_inode->i_ctime = current_time(root_inode);
    root_inode->i_private = assoofs_get_inode_info(sb, ASSOOFS_ROOTDIR_INODE_NUMBER);
    sb->s_root = d_make_root(root_inode);

    brelse(bh);
    return 0;
}

struct assoofs_inode_info *assoofs_get_inode_info(struct super_block *sb, uint64_t inode_no)
{
    // Paso 1
    struct assoofs_inode_info *inode_info;
    struct buffer_head *bh;
    struct assoofs_inode_info *buffer;
    struct assoofs_super_block_info *afs_sb;
    int i;

    bh = sb_bread(sb, ASSOOFS_INODESTORE_BLOCK_NUMBER);
    inode_info = (struct assoofs_inode_info *)bh->b_data;

    // PASO 2
    afs_sb = sb->s_fs_info;
    for (i = 0; i < afs_sb->inodes_count; i++)
    {
        if (inode_info->inode_no == inode_no)
        {
            buffer = kmalloc(sizeof(struct assoofs_inode_info), GFP_KERNEL);
            memcpy(buffer, inode_info, sizeof(*buffer));
            break;
        }
        inode_info++;
    }

    // PASO 3
    brelse(bh);
    return buffer;
}

/*
 *  Montaje de dispositivos assoofs
 */
static struct dentry *assoofs_mount(struct file_system_type *fs_type, int flags, const char *dev_name, void *data)
{
    struct dentry *ret;
    printk(KERN_INFO "assoofs_mount request\n");
    ret = mount_bdev(fs_type, flags, dev_name, data, assoofs_fill_super);
    // Control de errores a partir del valor de ret. En este caso se puede utilizar la macro IS_ERR: if (IS_ERR(ret)) ...

    return ret;
}

/*
 *  assoofs file system type
 */
static struct file_system_type assoofs_type = {
    .owner = THIS_MODULE,
    .name = "assoofs",
    .mount = assoofs_mount,
    .kill_sb = kill_block_super,
};

static int __init assoofs_init(void)
{
    int ret;
    printk(KERN_INFO "assoofs_init request\n");

    ret = register_filesystem(&assoofs_type);
    if (ret != 0)
    {
        printk(KERN_ERR "Error registering assoofs\n");
    }
    else
    {
        printk(KERN_INFO "assoofs registered\n");
    }

    // Control de errores a partir del valor de ret
    return ret;
}

static void __exit assoofs_exit(void)
{
    int ret;
    printk(KERN_INFO "assoofs_exit request\n");
    ret = unregister_filesystem(&assoofs_type);
    if (ret != 0)
    {

        printk(KERN_ERR "Error unregistering assoofs\n");
    }
    else
    {
        printk(KERN_INFO "assoofs unregistered\n");
    }
    // Control de errores a partir del valor de ret
}

module_init(assoofs_init);
module_exit(assoofs_exit);
