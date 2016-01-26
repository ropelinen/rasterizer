#include "software_rasterizer/precompiled.h"
#include "matrix.h"

#include "software_rasterizer/vector.h"

#include <math.h>

float mat34_get_det(const struct matrix_3x4 *mat)
{
	assert(mat && "mat34_get_det: mat is NULL");

	const float (*m)[3][4] = &(mat->mat);
	return ((*m)[0][0] * (*m)[1][1] * (*m)[2][2]) /* * 1) */ /* + ((*m)[0][0] * (*m)[1][2] * (*m)[2][3]  * 0) */ /* + ((*m)[0][0] * (*m)[1][3] * (*m)[2][1] * 0 */
	     /* + ((*m)[0][1] * (*m)[1][0] * (*m)[2][3]  * 0) */ + ((*m)[0][1] * (*m)[1][2] * (*m)[2][0]) /* * 1) */ /* + ((*m)[0][4] * (*m)[1][3] * (*m)[2][2] * 0 */
	     + ((*m)[0][2] * (*m)[1][0] * (*m)[2][1]) /* * 1) */ /* + ((*m)[0][2] * (*m)[1][1] * (*m)[2][3]  * 0) */ /* + ((*m)[0][2] * (*m)[1][3] * (*m)[2][0] * 0 */
	     /* + ((*m)[0][3] * (*m)[1][0] * (*m)[2][2]  * 0) */ /* + ((*m)[0][3] * (*m)[1][1] * (*m)[2][0]  * 0) */ /* + ((*m)[0][3] * (*m)[1][2] * (*m)[2][1] * 0 */
	     /* - ((*m)[0][0] * (*m)[1][1] * (*m)[2][3]  * 0) */ - ((*m)[0][0] * (*m)[1][2] * (*m)[2][1]) /* * 1) */ /* - ((*m)[0][0] * (*m)[1][3] * (*m)[2][2] * 0 */
	     - ((*m)[0][1] * (*m)[1][0] * (*m)[2][2]) /* * 1) */ /* - ((*m)[0][0] * (*m)[1][2] * (*m)[2][3]  * 0) */ /* - ((*m)[0][1] * (*m)[1][3] * (*m)[2][0] * 0 */
	     /* - ((*m)[0][2] * (*m)[1][0] * (*m)[2][3]  * 0) */ - ((*m)[0][2] * (*m)[1][1] * (*m)[2][0]) /* * 1) */ /* - ((*m)[0][2] * (*m)[1][3] * (*m)[2][1] * 0 */
	     /* - ((*m)[0][3] * (*m)[1][0] * (*m)[2][1]  * 0) */ /* - ((*m)[0][3] * (*m)[1][1] * (*m)[2][2]  * 0) */ /* - ((*m)[0][3] * (*m)[1][2] * (*m)[2][0] * 0 */;
}

struct matrix_3x4 mat34_get_inverse(const struct matrix_3x4 *mat)
{
	assert(mat && "mat34_get_inverse: mat is NULL");

	float det = mat34_get_det(mat);
	assert(det && "mat34_get_inverse: determinant is 0");

	const float(*m)[3][4] = &(mat->mat);
	struct matrix_3x4 result;
	result.mat[0][0] = ((*m)[1][1] * (*m)[2][2]) - ((*m)[1][2] * (*m)[2][1]);
	result.mat[0][1] = ((*m)[0][2] * (*m)[2][1]) - ((*m)[0][1] * (*m)[2][2]);
	result.mat[0][2] = ((*m)[0][1] * (*m)[1][2]) - ((*m)[0][2] * (*m)[1][1]);
	result.mat[0][3] = ((*m)[0][1] * (*m)[1][3] * (*m)[2][2]) + ((*m)[0][2] * (*m)[1][1] * (*m)[2][3]) + ((*m)[0][3] * (*m)[1][2] * (*m)[2][1])
	                 - ((*m)[0][1] * (*m)[1][2] * (*m)[2][3]) - ((*m)[0][2] * (*m)[2][3] * (*m)[2][1]) - ((*m)[0][3] * (*m)[1][1] * (*m)[2][2]);
	result.mat[1][0] = ((*m)[1][2] * (*m)[2][0]) - ((*m)[1][0] * (*m)[2][2]);
	result.mat[1][1] = ((*m)[0][0] * (*m)[2][2]) - ((*m)[0][2] * (*m)[2][0]);
	result.mat[1][2] = ((*m)[0][2] * (*m)[1][0]) - ((*m)[0][0] * (*m)[1][2]);
	result.mat[1][3] = ((*m)[0][0] * (*m)[1][2] * (*m)[2][3]) + ((*m)[0][2] * (*m)[1][3] * (*m)[2][0]) + ((*m)[0][3] * (*m)[1][0] * (*m)[2][2])
	                 - ((*m)[0][0] * (*m)[1][3] * (*m)[2][2]) - ((*m)[0][2] * (*m)[1][0] * (*m)[2][3]) - ((*m)[0][3] * (*m)[1][2] * (*m)[2][0]);
	result.mat[2][0] = ((*m)[1][0] * (*m)[2][1]) - ((*m)[1][1] * (*m)[2][0]);
	result.mat[2][1] = ((*m)[0][1] * (*m)[2][0]) - ((*m)[0][0] * (*m)[2][1]);
	result.mat[2][2] = ((*m)[0][0] * (*m)[1][1]) - ((*m)[0][1] * (*m)[1][0]);
	result.mat[2][3] = ((*m)[0][0] * (*m)[1][3] * (*m)[2][1]) + ((*m)[0][1] * (*m)[1][0] * (*m)[2][3]) + ((*m)[0][3] * (*m)[1][1] * (*m)[2][0])
	                 - ((*m)[0][0] * (*m)[1][1] * (*m)[2][3]) - ((*m)[0][1] * (*m)[1][3] * (*m)[2][0]) - ((*m)[0][3] * (*m)[1][0] * (*m)[2][1]);

	result = mat34_mul_scal(&result, 1/det);

	return result;
}

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

struct matrix_3x4 mat34_mul_scal(const struct matrix_3x4 *mat, float scal)
{
	assert(mat && "mat34_mul_scal: mat is NULL");

	struct matrix_3x4 result;

	result.mat[0][0] = mat->mat[0][0] * scal;
	result.mat[0][1] = mat->mat[0][1] * scal;
	result.mat[0][2] = mat->mat[0][2] * scal;
	result.mat[0][3] = mat->mat[0][3] * scal;
	result.mat[1][0] = mat->mat[1][0] * scal;
	result.mat[1][1] = mat->mat[1][1] * scal;
	result.mat[1][2] = mat->mat[1][2] * scal;
	result.mat[1][3] = mat->mat[1][3] * scal;
	result.mat[2][0] = mat->mat[2][0] * scal;
	result.mat[2][1] = mat->mat[2][1] * scal;
	result.mat[2][2] = mat->mat[2][2] * scal;
	result.mat[2][3] = mat->mat[2][3] * scal;

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

struct matrix_4x4 mat44_get_perspective_lh_fov(float fovy_rad, float aspect, float near, float far)
{
	struct matrix_4x4 result;

	float scaley = 1.0f / (tanf(fovy_rad / 2.0f));
	float scalex = scaley / aspect;
	float c = far / (far - near);

	result.mat[0][0] = scalex; result.mat[0][1] = 0.0f;   result.mat[0][2] = 0.0f; result.mat[0][3] = 0.0f;
	result.mat[1][0] = 0.0f;   result.mat[1][1] = scaley; result.mat[1][2] = 0.0f; result.mat[1][3] = 0.0f;
	result.mat[2][0] = 0.0f;   result.mat[2][1] = 0.0f;   result.mat[2][2] = c;    result.mat[2][3] = -near * c;
	result.mat[3][0] = 0.0f;   result.mat[3][1] = 0.0f;   result.mat[3][2] = 1.0f; result.mat[3][3] = 0.0f;

	return result;
}
struct matrix_4x4 mat44_get_perspective_lh(float width, float height, float near, float far)
{
	struct matrix_4x4 result;
	
	float d_n = 2.0f * near;
	float f_n = far - near;
	result.mat[0][0] = d_n  / width;   result.mat[0][1] = 0.0f;         result.mat[0][2] = 0.0f;      result.mat[0][3] = 0.0f;
	result.mat[1][0] = 0.0f;           result.mat[1][1] = d_n / height; result.mat[1][2] = 0.0f;      result.mat[1][3] = 0.0f;
	result.mat[2][0] = 0.0f;           result.mat[2][1] = 0.0f;         result.mat[2][2] = far / f_n; result.mat[2][3] = (near * far) / (near - far);
	result.mat[3][0] = 0.0f;           result.mat[3][1] = 0.0f;         result.mat[3][2] = 1.0f;      result.mat[3][3] = 0.0f;

	return result;
}

struct vec4_float mat44_mul_vec3(const struct matrix_4x4 *mat, const struct vec3_float *vec)
{
	assert(mat && "mat44_mul_vec3: mat is NULL");
	assert(vec && "mat44_mul_vec3: vec is NULL");

	struct vec4_float result;

	result.x = (mat->mat[0][0] * vec->x) + (mat->mat[0][1] * vec->y) + (mat->mat[0][2] * vec->z) + (mat->mat[0][3]/* * 1 */);
	result.y = (mat->mat[1][0] * vec->x) + (mat->mat[1][1] * vec->y) + (mat->mat[1][2] * vec->z) + (mat->mat[1][3]/* * 1 */);
	result.z = (mat->mat[2][0] * vec->x) + (mat->mat[2][1] * vec->y) + (mat->mat[2][2] * vec->z) + (mat->mat[2][3]/* * 1 */);
	result.w = (mat->mat[3][0] * vec->x) + (mat->mat[3][1] * vec->y) + (mat->mat[3][2] * vec->z) + (mat->mat[3][3]/* * 1 */);

	return result;
}

struct matrix_4x4 mat44_mul_mat34(const struct matrix_4x4 *mat1, const struct matrix_3x4 *mat2)
{
	assert(mat1 && "mat44_mul_mat34: mat1 is NULL");
	assert(mat2 && "mat44_mul_mat34: mat2 is NULL");

	struct matrix_4x4 result;

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

	result.mat[3][0] = mat1->mat[3][0] * mat2->mat[0][0] + mat1->mat[3][1] * mat2->mat[1][0] + mat1->mat[3][2] * mat2->mat[2][0] /* + mat1->mat[3][3] * 0 */;
	result.mat[3][1] = mat1->mat[3][0] * mat2->mat[0][1] + mat1->mat[3][1] * mat2->mat[1][1] + mat1->mat[3][2] * mat2->mat[2][1] /* + mat1->mat[3][3] * 0 */;
	result.mat[3][2] = mat1->mat[3][0] * mat2->mat[0][2] + mat1->mat[3][1] * mat2->mat[1][2] + mat1->mat[3][2] * mat2->mat[2][2] /* + mat1->mat[3][3] * 0 */;
	result.mat[3][3] = mat1->mat[3][0] * mat2->mat[0][3] + mat1->mat[3][1] * mat2->mat[1][3] + mat1->mat[3][2] * mat2->mat[2][3] + mat1->mat[3][3] /* * 1 */;

	return result;
}