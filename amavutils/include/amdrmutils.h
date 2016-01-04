#ifndef AMDRM_UTILS_H
#define AMDRM_UTILS_H

#ifdef  __cplusplus
extern "C" {
#endif

struct tvp_region
{
	uint64_t start;
	uint64_t end;
	int mem_flags;
};

extern int tvp_mm_enable(int flags);
extern int tvp_mm_disable(int flags);
extern int tvp_mm_get_mem_region(struct tvp_region* region, int region_size);


#ifdef  __cplusplus
}
#endif

#endif

