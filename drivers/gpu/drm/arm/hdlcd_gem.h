#ifndef __HDLCD_GEM_H__
#define __HDLCD_GEM_H__

int hdlcd_gem_dumb_create(struct drm_file *file_priv, struct drm_device *dev,
			  struct drm_mode_create_dumb *args);
struct drm_gem_object *hdlcd_gem_prime_import(struct drm_device *dev,
					      struct dma_buf *dma_buf);
struct dma_buf *hdlcd_gem_prime_export(struct drm_device *dev,
				      struct drm_gem_object *obj, int flags);
struct sg_table *hdlcd_gem_prime_get_sg_table(struct drm_gem_object *obj);
void *hdlcd_gem_prime_vmap(struct drm_gem_object *obj);
void hdlcd_gem_prime_vunmap(struct drm_gem_object *obj, void *vaddr);
int hdlcd_gem_prime_mmap(struct drm_gem_object *obj, struct vm_area_struct *vma);
int hdlcd_gem_mmap(struct file *filp, struct vm_area_struct *vma);
void hdlcd_gem_free_object(struct drm_gem_object *gem_obj);
int hdlcd_gem_create_ioctl(struct drm_device *dev, void *data,
			   struct drm_file *file_priv);

extern struct vm_operations_struct hdlcd_gem_vm_ops;

#endif /* __HDLCD_GEM_H__ */
