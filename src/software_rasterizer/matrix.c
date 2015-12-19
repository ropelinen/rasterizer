#include "software_rasterizer/precompiled.h"
#include "matrix.h"

#include "software_rasterizer/vector.h"

#include <math.h>

void mat34_set_to_indentity(struct matrix_3x4 *mat)
{
	assert(mat && "mat34_set_to_indentity: mat is NULL");

	mat->mat[0][0] = 1; mat->mat[0][1] = 0; mat->mat[0][2] = 0; mat->mat[0][3] = 0;
	mat->mat[1][0] = 0; mat->mat[1][1] = 1; mat->mat[1][2] = 0; mat->mat[1][3] = 0;
	mat->mat[2][0] = 0; mat->mat[2][1] = 0; mat->mat[2][2] = 1; mat->mat[2][3] = 0;
}

struct matrix_3x4 mat34_get_rotation_x(float rot_rad)
{
	struct matrix_3x4 result;

	result.mat[0][0] = 1; result.mat[0][1] = 0; result.mat[0][2] = 0; result.mat[0][3] = 0;
	result.mat[1][0] = 0; result.mat[1][1] = cosf(rot_rad); result.mat[1][2] = -sinf(rot_rad); result.mat[1][3] = 0;
	result.mat[2][0] = 0; result.mat[2][1] = sinf(rot_rad); result.mat[2][2] = cosf(rot_rad); result.mat[2][3] = 0;

	return result;
}

struct matrix_3x4 mat34_get_rotation_y(float rot_rad)
{
	struct matrix_3x4 result;

	result.mat[0][0] = cosf(rot_rad); result.mat[0][1] = 0; result.mat[0][2] = sinf(rot_rad); result.mat[0][3] = 0;
	result.mat[1][0] = 0; result.mat[1][1] = 1; result.mat[1][2] = 0; result.mat[1][3] = 0;
	result.mat[2][0] = -sinf(rot_rad); result.mat[2][1] = 0; result.mat[2][2] = cosf(rot_rad); result.mat[2][3] = 0;

	return result;
}

struct matrix_3x4 mat34_get_rotation_z(float rot_rad)
{
	struct matrix_3x4 result;

	result.mat[0][0] = cosf(rot_rad); result.mat[0][1] = -sinf(rot_rad); result.mat[0][2] = 0; result.mat[0][3] = 0;
	result.mat[1][0] = sinf(rot_rad); result.mat[1][1] = cosf(rot_rad); result.mat[1][2] = 0; result.mat[1][3] = 0;
	result.mat[2][0] = 0; result.mat[2][1] = 0; result.mat[2][2] = 1; result.mat[2][3] = 0;

	return result;
}

struct matrix_3x4 mat34_get_translation(const struct vec3_float *translation)
{
	assert(translation && "mat34_get_translation: translation is NULL");
	struct matrix_3x4 result;

	result.mat[0][0] = 1; result.mat[0][1] = 0; result.mat[0][2] = 0; result.mat[0][3] = translation->x;
	result.mat[1][0] = 0; result.mat[1][1] = 1; result.mat[1][2] = 0; result.mat[1][3] = translation->y;
	result.mat[2][0] = 0; result.mat[2][1] = 0; result.mat[2][2] = 1; result.mat[2][3] = translation->z;

	return result;
}

struct vec3_float mat34_mul_vec3(const struct matrix_3x4 *mat, const struct vec3_float *vec)
{
	assert(mat && "mat34_mul_vec3: mat is NULL");
	assert(vec && "mat34_mul_vec3: vec is NULL");

	struct vec3_float result;

	result.x = (mat->mat[0][0] * vec->x) + (mat->mat[0][1] * vec->y) + (mat->mat[0][2] * vec->z) + (mat->mat[0][3]/* * 1 */);
	result.y = (mat->mat[1][0] * vec->x) + (mat->mat[1][1] * vec->y) + (mat->mat[1][2] * vec->z) + (mat->mat[1][3]/* * 1 */);
	result.z = (mat->mat[2][0] * vec->x) + (mat->mat[2][1] * vec->y) + (mat->mat[2][2] * vec->z) + (mat->mat[2][3]/* * 1 */);

	return result;
}

struct matrix_3x4 mat34_mul_mat34(const struct matrix_3x4 *mat1, const struct matrix_3x4 *mat2)
{
	assert(mat1 && "mat34_mul_mat34: mat1 is NULL");
	assert(mat2 && "mat34_mul_mat34: mat2 is NULL");

	struct matrix_3x4 result;

	result.mat[0][0] = mat1->mat[0][0] * mat2->mat[0][0] + mat1->mat[0][1] * mat2->mat[1][0] + mat1->mat[0][2] * mat2->mat[2][0] /* + mat1->mat[0][3] * 0 */;
	result.mat[0][1] = mat1->mat[0][0] * mat2->mat[0][1] + mat1->mat[0][1] * mat2->mat[1][1] + mat1->mat[0][2] * mat2->mat[2][1] /* + mat1->mat[0][3] * 0 */;
	result.mat[0][2] = mat1->mat[0][0] * mat2->mat[0][2] + mat1->mat[0][1] * mat2->mat[1][2] + mat1->mat[0][2] * mat2->mat[2][2] /* + mat1->mat[0][3] * 0 */;
	result.mat[0][3] = mat1->mat[0][0] * mat2->mat[0][3] + mat1->mat[0][1] * mat2->mat[1][3] + mat1->mat[0][2] * mat2->mat[2][3] + mat1->mat[0][3] /* * 1 */;

	result.mat[1][0] = mat1->mat[1][0] * mat2->mat[0][0] + mat1->mat[1][1] * mat2->mat[1][0] + mat1->mat[1][2] * mat2->mat[2][0] /* + mat1->mat[1][3] * 0 */;
	result.mat[1][1] = mat1->mat[1][0] * mat2->mat[0][1] + mat1->mat[1][1] * mat2->mat[1][1] + mat1->mat[1][2] * mat2->mat[2][1] /* + mat1->mat[1][3] * 0 */;
	result.mat[1][2] = mat1->mat[1][0] * mat2->mat[0][2] + mat1->mat[1][1] * mat2->mat[1][2] + mat1->mat[1][2] * mat2->mat[2][2] /* + mat1->mat[1][3] * 0 */;
	result.mat[1][3] = mat1->mat[1][0] * mat2->mat[0][3] + mat1->mat[1][1] * mat2->mat[1][3] + mat1->mat[1][2] * mat2->mat[2][3] + mat1->mat[1][3] /* * 1 */;

	result.mat[2][0] = mat1->mat[2][0] * mat2->mat[0][0] + mat1->mat[2][1] * mat2->mat[1][0] + mat1->mat[2][2] * mat2->mat[2][0] /* + mat1->mat[2][3] * 0 */;
	result.mat[2][1] = mat1->mat[2][0] * mat2->mat[0][1] + mat1->mat[2][1] * mat2->mat[1][1] + mat1->mat[2][2] * mat2->mat[2][1] /* + mat1->mat[2][3] * 0 */;
	result.mat[2][2] = mat1->mat[2][0] * mat2->mat[0][2] + mat1->mat[2][1] * mat2->mat[1][2] + mat1->mat[2][2] * mat2->mat[2][2] /* + mat1->mat[2][3] * 0 */;
	result.mat[2][3] = mat1->mat[2][0] * mat2->mat[0][3] + mat1->mat[2][1] * mat2->mat[1][3] + mat1->mat[2][2] * mat2->mat[2][3] + mat1->mat[2][3] /* * 1 */;

	return result;
}