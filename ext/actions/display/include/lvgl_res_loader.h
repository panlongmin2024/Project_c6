#ifndef _LVGL_RES_LOADER_H
#define _LVGL_RES_LOADER_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include <lvgl.h>
#include <res_manager_api.h>

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

typedef struct
{
	/*! scene id*/
	uint32_t id;
	/*! ������*/
	lv_coord_t  x;
	/*! ������*/
	lv_coord_t  y;
	/*! ���	*/
	uint16_t  width;
	/*! �߶�	*/
	uint16_t  height;
	/*! ����ɫ*/
	lv_color_t background;
	/*! scene ��͸����*/
	uint32_t  transparence;
	/*! res manager�������ݽṹ��ָ��*/
	resource_scene_t* scene_data;
} lvgl_res_scene_t;

typedef struct
{
	/*! group id*/
	uint32_t id;
	/*! �������һ���ṹ��x����  */
	lv_coord_t x;
	/*! �������һ���ṹ��y����  */
	lv_coord_t y;
	/*! ���  */
	uint16_t width;
	/*! �߶�  */
	uint16_t height;
	/*! res manager��Դ�����ݽṹ��ָ��*/
	resource_group_t* group_data;
} lvgl_res_group_t;

typedef struct
{
	/*! �������һ���ṹ��x����  */
	lv_coord_t x;
	/*! �������һ���ṹ��y����  */
	lv_coord_t y;
	/*! ���  */
	uint16_t width;
	/*! �߶�  */
	uint16_t height;
	/*! ͼƬ��֡�� */
	uint32_t frames;
	/*! ͼƬ��id*/
	resource_picregion_t* picreg_data;
}lvgl_res_picregion_t;

typedef struct
{
	/*! ������ */
	lv_coord_t  x;
	/*! ������ */
	lv_coord_t  y;
	/*! ���	*/
	uint16_t  width;
	/*! �߶�	*/
	uint16_t  height;
	lv_color_t color;
	uint8_t* txt;
} lvgl_res_string_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/
/************************************************************************************/
/*!
 * \par  Description:
 *     �����Դ����
 * \return       ��
 * \note		
 ************************************************************************************/ 
void lvgl_res_cache_clear(void);

/************************************************************************************/
/*!
 * \par  Description:
 *     ����Դ����ʽ�ļ�
 * \param[in]    style_path   ��ʽ�ļ�·��
 * \param[in]    picture_path   ͼƬ��Դ�ļ�·��
 * \param[in]    text_path   �ı���Դ�ļ�·��
 * \return       �ɹ�����0��ʧ�ܷ���-1
 * \note		ϵͳ��ʼ��ʱ����һ��
 ************************************************************************************/
int lvgl_res_loader_open(const char* sty_file, const char* pic_file, const char* str_file);

/************************************************************************************/
/*!
 * \par  Description:
 *     �ر���Դ����ʽ�ļ�
 * \return       ��
 * \note		ϵͳ�ػ�ʱ����һ�� 
 ************************************************************************************/
void lvgl_res_loader_close(void);

/************************************************************************************/
/*!
 * \par  Description:
 *     ���س���
 * \param[in]    scene_id   ����id
 * \return       �ɹ����س�����Դ����ָ�룬ʧ�ܷ���NULL
 ************************************************************************************/
int lvgl_res_load_scene(uint32_t scene_id, lvgl_res_scene_t* scene);

/************************************************************************************/
/*!
 * \par  Description:
 *    	�ӳ����м�������Դ�� 
 * \param[in]    scene   ��������ָ��
 * \param[in]    id   	��Դ��id
 * \return       �ɹ�������Դ������ָ�룬ʧ�ܷ���NULL
 ************************************************************************************/
int lvgl_res_load_group_from_scene(lvgl_res_scene_t* scene, uint32_t id, lvgl_res_group_t* group);

/************************************************************************************/
/*!
 * \par  Description:
 *    	����Դ���м�����Դ��
 * \param[in]    group   ��Դ������ָ�� 
 * \param[in]    id   	��Դ��id
 * \return       �ɹ�������Դ������ָ�룬ʧ�ܷ���NULL
 ************************************************************************************/
int lvgl_res_load_group_from_group(lvgl_res_group_t* group, uint32_t id, lvgl_res_group_t* subgroup);

/************************************************************************************/
/*!
 * \par  Description:
 *    	�ӳ����м���ͼƬ
 * \param[in]    scene   ��������ָ��
 * \param[in]    id   	���ͼƬ��Դid������ָ�� 
 * \param[out]   img   	���lvglͼƬ���ݵ�����ָ�� 
 * \param[out]   pt   	���ͼƬ���������ָ�� 
 * \param[in]	 num	����Ԫ�ظ���
 * \return       �ɹ�����0��ʧ�ܷ���-1
 ************************************************************************************/
int lvgl_res_load_pictures_from_scene(lvgl_res_scene_t* scene, const uint32_t* id, lv_img_dsc_t* img, lv_point_t* pt, uint32_t num);

/************************************************************************************/
/*!
 * \par  Description:
 *    	�ӳ����м����ַ��� 
 * \param[in]    scene   ��������ָ��
 * \param[in]    id   	����ַ�����Դid������ָ�� 
 * \param[out]   str   	�������lvgl��ʾ���ַ������ݵ�����ָ�� 
 * \param[in]	 num	����Ԫ�ظ���
 * \return       �ɹ�����0��ʧ�ܷ���-1
 ************************************************************************************/
int lvgl_res_load_strings_from_scene(lvgl_res_scene_t* scene, const uint32_t* id, lvgl_res_string_t* str, uint32_t num);

/************************************************************************************/
/*!
 * \par  Description:
 *    	����Դ���м���ͼƬ 
 * \param[in]    group  ��Դ������ָ��  
 * \param[in]    id   	���ͼƬ��Դid������ָ�� 
 * \param[out]   img   	���lvglͼƬ���ݵ�����ָ�� 
 * \param[out]   pt   	���ͼƬ���������ָ�� 
 * \param[in]	 num	����Ԫ�ظ���
 * \return       �ɹ�����0��ʧ�ܷ���-1
 ************************************************************************************/
int lvgl_res_load_pictures_from_group(lvgl_res_group_t* group, const uint32_t* id, lv_img_dsc_t* img, lv_point_t* pt, uint32_t num);

/************************************************************************************/
/*!
 * \par  Description:
 *    	����Դ���м����ַ���
 * \param[in]    group  ��Դ������ָ�� 
 * \param[in]    id   	����ַ�����Դid������ָ�� 
 * \param[out]   str   	�������lvgl��ʾ���ַ������ݵ�����ָ�� 
 * \param[in]	 num	����Ԫ�ظ���
 * \return       �ɹ�����0��ʧ�ܷ���-1
 ************************************************************************************/
int lvgl_res_load_strings_from_group(lvgl_res_group_t* group, const uint32_t* id, lvgl_res_string_t* str, uint32_t num);

/************************************************************************************/
/*!
 * \par  Description:
 *     ж��ͼƬ��Դ
 * \param[in]    img   	���lvglͼƬ���ݵ�����ָ�� 
 * \param[in]	 num	����Ԫ�ظ���
 * \return       �� 
 * \note		���ٻ����е����ü�����������ü�����0������Ϊ������͓�
 ************************************************************************************/
void lvgl_res_unload_pictures(lv_img_dsc_t* img, uint32_t num);

/************************************************************************************/
/*!
 * \par  Description:
 *     ж���ַ����ʜ�
 * \param[in]    img   	����ַ������ݵ�����ָ��
 * \param[in]	 num	����Ԫ�ظ���
 * \return       �� 
 * \note		���ٻ����е����ü�����������ü�����0������Ϊ������ͷ� 
 ************************************************************************************/
void lvgl_res_unload_strings(lvgl_res_string_t* str, uint32_t num);

/************************************************************************************/
/*!
 * \par  Description:
 *     �ͷ���Դ�����ݿռ� 
 * \return       ��
 ************************************************************************************/
void lvgl_res_unload_group(lvgl_res_group_t* group);

/************************************************************************************/
/*!
 * \par  Description:
 *     �ͷų������ݿռ�
 * \return       �� 
 ************************************************************************************/
void lvgl_res_unload_scene(lvgl_res_scene_t* scene);

/************************************************************************************/
/*!
 * \par  Description:
 *     �ͷ�ͼƬ�������ݿռ� 
 * \return       �� 
 ************************************************************************************/
void lvgl_res_unload_picregion(lvgl_res_picregion_t* picreg);

/************************************************************************************/
/*!
 * \par  Description:
 *    	����Դ���м���ͼƬ���� 
 * \param[in]    group  ��Դ������ָ�� 
 * \param[in]    id   	ͼƬ����id
 * \param[in]	 res_picreg	ͼƬ��Դ���ݽṹָ�� 
 * \return       �ɹ�����0��ʧ�ܷ���-1 
 ************************************************************************************/
int lvgl_res_load_picregion_from_group(lvgl_res_group_t* group, const uint32_t id, lvgl_res_picregion_t* res_picreg);

/************************************************************************************/
/*!
 * \par  Description:
 *    	�ӳ����м���ͼƬ����
 * \param[in]    scene  ��������ָ��
 * \param[in]    id   	ͼƬ����id
 * \param[in]	 res_picreg	ͼƬ��Դ���ݽṹָ��
 * \return       �ɹ�����0��ʧ�ܷ���-1 
 ************************************************************************************/
int lvgl_res_load_picregion_from_scene(lvgl_res_scene_t* scene, const uint32_t id, lvgl_res_picregion_t* res_picreg);

/************************************************************************************/
/*!
 * \par  Description:
 *    	��ͼƬ�����м���ͼƬ
 * \param[in]    picreg  ͼƬ��������ָ��
 * \param[in]    start   Ҫ���ص���ʼ֡��
 * \param[in]	 end	 Ҫ���صĽ���֡��
 * \param[out]   img   	���lvglͼƬ���ݵ�����ָ�� 
 * \return       �ɹ�����0��ʧ�ܷ���-1 
 ************************************************************************************/
int lvgl_res_load_pictures_from_picregion(lvgl_res_picregion_t* picreg, uint32_t start, uint32_t end, lv_img_dsc_t* img);

/************************************************************************************/
/*!
 * \par  Description:
 *    	�ӳ�����Ԥ����ͼƬ 
 * \param[in]    scene  ��������ָ��
 * \param[in]    id   	���ͼƬ��Դid������ָ�� 
 * \param[in]	 num	����Ԫ�ظ���
 * \return       �ɹ�����0��ʧ�ܷ���-1 
 ************************************************************************************/
int lvgl_res_preload_pictures_from_scene(lvgl_res_scene_t* scene, const uint32_t* id, uint32_t num);

/************************************************************************************/
/*!
 * \par  Description:
 *    	����Դ����Ԥ����ͼƬ
 * \param[in]    group  ��Դ������ָ�� 
 * \param[in]    id   	���ͼƬ��Դid������ָ�� 
 * \param[in]	 num	����Ԫ�ظ���
 * \return       �ɹ�����0��ʧ�ܷ���-1 
 ************************************************************************************/
int lvgl_res_preload_pictures_from_group(lvgl_res_group_t* group, const uint32_t* id, uint32_t num);

/************************************************************************************/
/*!
 * \par  Description:
 *    	��ͼƬ������Ԥ����ͼƬ 
 * \param[in]    picreg  ͼƬ��������ָ��
 * \param[in]    start   Ҫ���ص���ʼ֡��
 * \param[in]	 end	 Ҫ���صĽ���֡��
 * \return       �ɹ�����0��ʧ�ܷ���-1 
 ************************************************************************************/
int lvgl_res_preload_pictures_from_picregion(lvgl_res_picregion_t* picreg, uint32_t start, uint32_t end);

/************************************************************************************/
/*!
 * \par  Description:
 *     ȡ����ԴԤ����
 * \return       �� 
 ************************************************************************************/
int lvgl_res_preload_cancel(void);

/************************************************************************************/
/*!
 * \par  Description:
 *    	��Ĭ��·������ͼƬ 
 * \param[in]    group_id  ��һ����Դ��id��Ϊ��ѡ�������ʱ��0
 * \param[in]    subgroup_id  �ڶ�����Դ��id��Ϊ��ѡ�������ʱ��0 
 * \param[in]    id   	���ͼƬ��Դid������ָ�룬Ϊ������ 
 * \param[out]   img   	���lvglͼƬ���ݵ�����ָ�� 
 * \param[out]   pt   	���ͼƬ���������ָ�� 
 * \param[in]	 num	����Ԫ�ظ���
 * \return       �ɹ�����0��ʧ�ܷ���-1
 * \note		Ĭ��·���ӳ���id��ʼ��������Դid���������֧���м�������Դ��
 ************************************************************************************/
int lvgl_res_load_pictures(uint32_t group_id, uint32_t subgroup_id, uint32_t* id, lv_img_dsc_t* img, lv_point_t* pt, uint32_t num);

/************************************************************************************/
/*!
 * \par  Description:
 *    	��Ĭ��·�������ַ���
 * \param[in]    group_id  ��һ����Դ��id��Ϊ��ѡ�������ʱ��0
 * \param[in]    subgroup_id  �ڶ�����Դ��id��Ϊ��ѡ�������ʱ��0 
 * \param[in]    id   	����ַ�����Դid������ָ�룬Ϊ������ 
 * \param[out]   str   	�������lvgl��ʾ���ַ������ݵ�����ָ�� 
 * \param[in]	 num	����Ԫ�ظ���
 * \return       �ɹ�����0��ʧ�ܷ���-1
 * \note		Ĭ��·���ӳ���id��ʼ��������Դid���������֧���м�������Դ�� 
 ************************************************************************************/
int lvgl_res_load_strings(uint32_t group_id, uint32_t subgroup_id, uint32_t* id, lvgl_res_string_t* str, uint32_t num);

/************************************************************************************/
/*!
 * \par  Description:
 *    	ʹ��Ĭ��·������ͼƬ���� 
 * \param[in]    group_id  ��һ����Դ��id��Ϊ��ѡ�������ʱ��0
 * \param[in]    subgroup_id  �ڶ�����Դ��id��Ϊ��ѡ�������ʱ��0 
 * \param[in]    id   	ͼƬ����id��Ϊ������
 * \param[in]	 res_picreg	ͼƬ��Դ���ݽṹָ�� 
 * \return       �ɹ�����0��ʧ�ܷ���-1 
 * \note		Ĭ��·���ӳ���id��ʼ��������Դid���������֧���м�������Դ�� 
 ************************************************************************************/
int lvgl_res_load_picregion(uint32_t group_id, uint32_t subgroup_id, uint32_t picreg_id, lvgl_res_picregion_t* res_picreg);

/************************************************************************************/
/*!
 * \par  Description:
 *    	����Ĭ�ϳ��� 
 * \param[in]    scene_id  Ĭ�ϳ���id
 * \return       �ɹ�����0��ʧ�ܷ���-1
 ************************************************************************************/
int lvgl_res_set_default_scene(uint32_t scene_id);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*_LVGL_RES_LOADER_H*/

