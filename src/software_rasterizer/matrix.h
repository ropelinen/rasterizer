#ifndef RPLNN_MATRIX_H
#define RPLNN_MATRIX_H

struct matrix_3x4
{
	float mat[3][4];
};

struct matrix_4x4
{
	float mat[4][4];
};

float mat34_get_det(const struct matrix_3x4 *mat);
struct matrix_3x4 mat34_get_inverse(const struct matrix_3x4 *mat);

void mat34_set_to_indentity(struct matrix_3x4 *mat);
struct matrix_3x4 mat34_get_rotation_x(float rot_rad);
struct matrix_3x4 mat34_get_rotation_y(float rot_rad);
struct matrix_3x4 mat34_get_rotation_z(float rot_rad);
struct matrix_3x4 mat34_get_translation(const struct vec3_float *translation);

struct matrix_3x4 mat34_mul_scal(const struct matrix_3x4 *mat, float scal);
struct vec3_float mat34_mul_vec3(const struct matrix_3x4 *mat, const struct vec3_float *vec);
struct matrix_3x4 mat34_mul_mat34(const struct matrix_3x4 *mat1, const struct matrix_3x4 *mat2);

struct matrix_4x4 mat44_get_perspective_lh_fov(float fovy_rad, float aspect, float near, float far);
struct matrix_4x4 mat44_get_perspective_lh(float width, float height, float near, float far);
struct vec4_float mat44_mul_vec3(const struct matrix_4x4 *mat, const struct vec3_float *vec);
struct matrix_4x4 mat44_mul_mat34(const struct matrix_4x4 *mat1, const struct matrix_3x4 *mat2);

#endif /* RPLNN_MATRIX_H */
