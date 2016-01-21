#ifndef RPLNN_STATS_H
#define RPLNN_STATS_H

enum stat_types
{
	STAT_FRAME = 0,
	STAT_BLIT,
	STAT_RASTER,
	STAT_COUNT
};

struct stats;

/* Should create a version of this which doesn't malloc (basically just give memory block as a parameter). */
struct stats *stats_create(const unsigned char stat_count, const unsigned int frames_in_buffer, const bool profilingRun);
void stats_destroy(struct stats **stats);

bool stats_profiling_run_complete(const struct stats *stats);

/* Call this only once per stat between stat_frame_complete calls.
 * Calling this multiple times for a single stat overrides the previous value. */
void stats_update_stat(struct stats *stats, const unsigned char stat_id, const uint32_t time);
void stats_frame_complete(struct stats *stats);

uint32_t stats_get_stat_prev_frame(struct stats *stats, const unsigned char stat_id);
/* Returns stats for the previous frame */
uint32_t stats_get_stat_percentile(struct stats *stats, const unsigned char stat_id, const float percentile);
uint32_t stats_get_avarage(struct stats *stats, const unsigned char stat_id);


#endif /* RPLNN_STATS_H */
