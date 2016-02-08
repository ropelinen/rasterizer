#include "software_rasterizer/precompiled.h"

#include "stats.h"

#include <string.h>

struct stats
{
	/* When using microseconds uint32_t can store 71 minutes, 
	 * uint16_would be enough for 65 milliseconds. 
	 * Could store the time in 1/10 milliseconds or something like,
	 * it should be ok for any normal frame time but not worth the trouble right now. */
	uint32_t *stats;
	/* When ever a stat is updated the value is also added to this sorted array and the old value is removed.
	 * This allows efficient checking of percentile values. However this doubles memory consumption. */
	uint32_t *stats_sorted;
	/* The sum of all the stats, used for fast calculation of avarages. */
	uint64_t *sums;
	/* We use the stats array like a circular buffer
	 * this is the index we are currently using. */
	unsigned int current_index;
	unsigned int frames_in_buffer;
	unsigned char stat_count;
	/* If set to true the stats are collected only until the buffer gets full.
	 * Ie the index will not loop (nor is the last stat overwritten). */
	bool profiling_run;
};

struct stats *stats_create(const unsigned char stat_count, const unsigned int frames_in_buffer, const bool profiling_run)
{
	struct stats *stats = malloc(sizeof(struct stats));

	stats->stats = malloc(stat_count * frames_in_buffer * sizeof(uint32_t));
	memset(stats->stats, 0, stat_count * frames_in_buffer * sizeof(uint32_t));
	stats->stats_sorted = malloc(stat_count * frames_in_buffer * sizeof(uint32_t));
	memset(stats->stats_sorted, 0, stat_count * frames_in_buffer * sizeof(uint32_t));
	stats->sums = malloc(stat_count * sizeof(uint64_t));
	memset(stats->sums, 0, stat_count * sizeof(uint64_t));

	stats->current_index = 0;
	stats->frames_in_buffer = frames_in_buffer;
	stats->stat_count = stat_count;
	stats->profiling_run = profiling_run;

	return stats;
}

void stats_destroy(struct stats **stats)
{
	assert(stats && "stats_destroy: stats is NULL");
	assert(*stats && "stats_destroy: *stats is NULL");

	free((*stats)->sums);
	free((*stats)->stats_sorted);
	free((*stats)->stats);
	free(*stats);
}

bool stats_profiling_run_complete(const struct stats *stats)
{
	assert(stats && "stats_profiling_run_complete: stats is NULL");
	assert(stats->profiling_run && "stats_profiling_run_complete: The stats are not defined as a profiling run");

	return stats->profiling_run && stats->current_index >= stats->frames_in_buffer;
}

void update_sorted(uint32_t *data, unsigned int data_size, const uint32_t new_val, const uint32_t prev_val)
{
	assert(data && "update_sorted: data is NULL");

	unsigned int index = ~0U;
	for (unsigned i = 0; i < data_size; ++i)
	{
		if (data[i] == prev_val)
		{
			index = i;
			break;
		}
	}

	assert(index != ~0U && "update_sorted: prev_val not found");
	if (index == ~0U)
		return;

	/* Should sort these from smallest to largest instead. (remember to change get percentile func too. */
	if (new_val > prev_val)
	{
		if (index == 0)
		{
			data[index] = new_val;
			return;
		}

		while (index > 0)
		{
			if (data[index - 1] >= new_val)
			{
				data[index] = new_val;
				return;
			}
			else
			{
				data[index] = data[index - 1];
				--index;
				if (index == 0)
				{
					data[0] = new_val;
					return;
				}
			}
		}
	}
	else
	{
		if (index == data_size - 1)
		{
			data[index] = new_val;
			return;
		}

		while (index < data_size - 1)
		{
			if (data[index + 1] <= new_val)
			{
				data[index] = new_val;
				return;
			}
			else
			{
				data[index] = data[index + 1];
				++index;
				if (index == data_size - 1)
				{
					data[index] = new_val;
					return;
				}
			}
		}
	}
}

void stats_update_stat(struct stats *stats, const unsigned char stat_id, const uint32_t time)
{
	assert(stats && "stats_update_stat: stats is NULL");
	assert(stat_id < stats->stat_count && "stats_update_stat: Too big stat_id");

	if (stats->profiling_run && stats->current_index >= stats->frames_in_buffer)
		return;

	uint32_t prev_value = stats->stats[(stat_id * stats->frames_in_buffer) + stats->current_index];
	/* No change here, don't do anything. */
	if (time == prev_value)
		return;
	
	stats->stats[(stat_id * stats->frames_in_buffer) + stats->current_index] = time;

	stats->sums[stat_id] -= prev_value;
	stats->sums[stat_id] += time;

	update_sorted(&(stats->stats_sorted[stat_id * stats->frames_in_buffer]), stats->frames_in_buffer, time, prev_value);
#if RPLNN_BUILD_TYPE == RPLNN_DEBUG
	uint32_t *data = &(stats->stats_sorted[stat_id * stats->frames_in_buffer]);
	for (unsigned int i = 0; i < stats->frames_in_buffer - 1; ++i)
		assert(data[i] >= data[i + 1] && "sort: stats not properly sorted after running insertion");
#endif
}

void stats_frame_complete(struct stats *stats)
{
	assert(stats && "stats_frame_complete: stats is NULL");

	++(stats->current_index);
	if (stats->current_index >= stats->frames_in_buffer)
	{
		if (stats->profiling_run)
			stats->current_index = stats->frames_in_buffer;
		else
			stats->current_index = 0;
	}
}

uint32_t stats_get_stat_prev_frame(struct stats *stats, const unsigned char stat_id)
{
	assert(stats && "stats_get_stat_prev_frame: stats in NULL");

	return stats->stats[(stat_id * stats->frames_in_buffer) + (stats->current_index ? stats->current_index : stats->frames_in_buffer) - 1];
}

uint32_t stats_get_stat_percentile(struct stats *stats, const unsigned char stat_id, const float percentile)
{
	assert(stats && "stats_get_stat_percentile: stats in NULL");

	unsigned int index = (unsigned int)((stats->frames_in_buffer - 1) * (1.0f - (percentile * 0.01f)));
	return stats->stats_sorted[(stat_id * stats->frames_in_buffer) + index];
}

uint32_t stats_get_avarage(struct stats *stats, const unsigned char stat_id)
{
	assert(stats && "stats_get_avarage: stats in NULL");
	return (uint32_t)(stats->sums[stat_id] / stats->frames_in_buffer);
}
