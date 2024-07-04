#ifndef __RES_MANAGER_API_H__
#define __RES_MANAGER_API_H__

#include <fs/fs.h>

typedef enum 
{
	RESOURCE_TYPE_GROUP=3,
	RESOURCE_TYPE_PICTURE=4,
	RESOURCE_TYPE_TEXT=2,
	RESOURCE_TYPE_PICREGION=1, //fixme
} resource_type_e;

typedef enum
{
	RESOURCE_BITMAP_FORMAT_RGB565,
	RESOURCE_BITMAP_FORMAT_ARGB8565,
	RESOURCE_BITMAP_FORMAT_RGB888,
	RESOURCE_BITMAP_FORMAT_ARGB8888,
} resource_bitmap_format_e;

/*!
*\brief
    ��ǰӦ����Դ�ļ��ܵ����ݽṹ
*/
typedef struct style_s
{
	/*! style�ļ����*/
	struct fs_file_t style_fp;
	/*! ͼƬ��Դ�ļ����*/
	struct fs_file_t pic_fp;
	/*! �ı���Դ�ļ����*/
	struct fs_file_t text_fp;
	/*! ���������� */
	uint32_t sum; 
	/*! ���������� */
	int32_t* scenes;
} resource_info_t;

/*!
*\brief
    ���������ݽṹ
*/
typedef struct
{		  
	/*! ������  */
	int16_t  x;
	/*! ������  */
	int16_t  y;
	/*! ���	*/
	int16_t  width;
	/*! �߶�	*/
	int16_t  height;
	/*! ����ɫ  */
	uint32_t background;	 
	/*! scene �Ƿ�ɼ�	*/
	uint8_t  visible;
	/*! scene �Ƿ�͸�� */
	uint8_t   opaque;
	/*! scene ��͸����	*/
	uint16_t  transparence;
	
	/*! ��scene �к�����Դ����Ŀ*/
	uint32_t resource_sum;
	/*! ���ӵ�ƫ���� */
	uint32_t child_offset;
	/*! ��������*/
	uint8_t direction;
	uint8_t keys[ 16 ];
} resource_scene_t;


/*!
*\brief
    ��Դ������ݽṹ
*/
typedef struct
{
	/*!��Դ����*/
	uint32_t type;
	/*! �������һ���ṹ��x����  */
	int32_t x;
	/*! �������һ���ṹ��y����  */
	int32_t y;   
	/*! ���  */
	uint32_t width;
	/*! �߶�  */
	uint32_t height;
	/*! ����Դ������Դ����Ŀ*/
	uint32_t resource_sum;
	/*! ����Դ������Դ��ƫ�ƣ��ڲ�������Դ�ã������߲���Ҫ����*/
	uint32_t child_offset;	
	/*! ����Դ���Ӧ�ĵ���Դ����ԭ�㣬�����߲���Ҫ����*/
	uint32_t start;
} resource_group_t;

/*!
*\brief
    ͼƬ��Դ�����ݽṹ
*/
typedef struct
{
	/*!��Դ����*/
	uint32_t type;	
	/*!��Դid*/
	uint32_t id;
	/*! �������һ���ṹ��x����  */
	int32_t x;
	/*! �������һ���ṹ��y����  */
	int32_t y;
	/*! ���  */
	uint32_t width;
	/*! �߶�  */
	uint32_t height;
	/*! ÿ�����ֽ���  */
	uint32_t bytes_per_pixel;
	/*! λͼ��ʽ  */
	uint32_t format;	
	/*! λͼbuffer  */
	uint8_t* buffer;
} resource_bitmap_t;

/*!
*\brief
    �ַ�����Դ�����ݽṹ
*/
typedef struct
{
	/*!��Դ����*/
	uint32_t type;
	/*!��Դid*/
	uint32_t id;
	/*! �������һ���ṹ��x����  */
	int32_t x;
	/*! �������һ���ṹ��y����  */
	int32_t y;
	/*! ���  */
	uint32_t width;
	/*! �߶�  */
	uint32_t height;
	/*! �����С  */
	uint32_t font_size;
	/*! �ı���ɫ  */
	uint32_t color;
	/*! �ı����ݣ�Ĭ��utf8���룩*/
	uint8_t* buffer;	
} resource_text_t;

/*!
 *\brief
 	ͼƬ����Դ�����ݽṹ
*/
typedef struct
{
	uint32_t type;
 	int32_t x;
	int32_t y;
	uint32_t width;
	uint32_t height;
	uint32_t frames;
	uint32_t pic_offset;
	uint32_t start;
	uint32_t* id;
}resource_picregion_t;


void res_manager_init(void);

/************************************************************************************/
/*!
 * \par  Description:
 *     �����Դ����
 * \param[in]    �� 
 * \return       ��
 ************************************************************************************/
void res_manager_clear_cache(void);

/************************************************************************************/
/*!
 * \par  Description:
 *     ����Դ����ʽ�ļ�
 * \param[in]    style_path   ��ʽ�ļ�·�� 
 * \param[in]    picture_path   ͼƬ��Դ�ļ�·��
 * \param[in]    text_path   �ı���Դ�ļ�·��
 * \return       �ɹ�������Դ����ָ�룬ʧ�ܷ���NULL
 * \note	��ʽ�ļ����ͼƬ���ı���Դ�Ĳ�����Ϣ�����ճ���->��Դ��->��Դ�Ľṹ��š�
 *			ui edit������һ��ͷ�ļ���������ʽ�ļ��еĳ�������Դ�����Դ��id���궨�壩��
 *			����������ڴ��������ã����id����Դ�ļ��е���Դid�ǲ�һ���ġ�
 *			ͼƬ��Դ�ļ����ͼƬ��Դ�ľ������ݡ�
 *			�ı���Դ�ļ�����ַ����ı���Դ�ľ������ݡ�
 ************************************************************************************/
resource_info_t* res_manager_open_resources( const char* style_path, const char* picture_path, const char* text_path );

/************************************************************************************/
/*!
 * \par  Description:
 *     �ر���Դ����ʽ�ļ�
 * \param[in]    res_info   ��Դ����ָ�� 
 * \return       ��
 ************************************************************************************/
void res_manager_close_resources( resource_info_t* res_info );

/************************************************************************************/
/*!
 * \par  Description:
 *     ���س���
 * \param[in]    res_info   ��Դ����ָ�� 
 * \param[in]    scene_id   ����id
 * \return       �ɹ����س�������ָ�룬ʧ�ܷ���NULL
 ************************************************************************************/
resource_scene_t* res_manager_load_scene(resource_info_t* res_info, uint32_t scene_id);

/************************************************************************************/
/*!
 * \par  Description:
 *     �ͷų���
 * \param[in]    scene   ��������ָ�� 
 * \return       ��
 ************************************************************************************/
void res_manager_unload_scene(resource_scene_t* scene);

/************************************************************************************/
/*!
 * \par  Description:
 *     ���س���
 * \param[in]    scene   ��������ָ�� 
 * \param[in]    id   ��Դ����Դ��id�����id�Ƕ�����ui editor���ɵ�ͷ�ļ��ڵġ�
 * \return       �ɹ�������Դ����Դ������ָ�룬ʧ�ܷ���NULL
 * \note	���ص�ʵ�����������Ǹ���id����Ӧ����Դ�����������ģ���Ϊ������Ӧ��֪��
 *			��Դ�����ͣ���˶Է��ص�ָ���������ת�����ɡ������µ�����Դ��������Դ�飬
 *			ͼƬ��Դ�����ַ����ı���Դ��
 *			�����ȡ��ͼƬ�����ı���Դ���λͼ���ַ����������ݼ��ص������С�
 ************************************************************************************/
void* res_manager_get_scene_child(resource_info_t*res_info, resource_scene_t* scene, uint32_t id);

/************************************************************************************/
/*!
 * \par  Description:
 *     ���س���
 * \param[in]    resgroup   ��Դ������ָ�� 
 * \param[in]    id   ��Դ����Դ��id�����id�Ƕ�����ui editor���ɵ�ͷ�ļ��ڵġ�
 * \return       �ɹ�������Դ����Դ������ָ�룬ʧ�ܷ���NULL
 * \note	���ص�ʵ�����������Ǹ���id����Ӧ����Դ�����������ģ���Ϊ������Ӧ��֪��
 *			��Դ�����ͣ���˶Է��ص�ָ���������ת�����ɡ� �����µ�����Դ��������Դ�飬
 *			ͼƬ��Դ�����ַ����ı���Դ��
 *			�����ȡ��ͼƬ�����ı���Դ���λͼ���ַ����������ݼ��ص������С�
 *	**********************************************************************************/
void* res_manager_get_group_child(resource_info_t*res_info, resource_group_t* resgroup, uint32_t id );

/************************************************************************************/
/*!
 * \par  Description:
 *     �ͷ���Դ����
 * \param[in]    resource   ��Դ������Դ������ָ�� 
 * \return       ��
 ************************************************************************************/
void res_manager_release_resource(void* resource);

/************************************************************************************/
/*!
 * \par  Description:
 *     �ͷ���Դ���ݽṹ��
 * \param[in]    resource   ��Դ������Դ������ָ�� 
 * \return       ��
 ************************************************************************************/
void res_manager_free_resource_structure(void* resource);

/************************************************************************************/
/*!
 * \par  Description:
 *     �ͷ�ͼƬ��Դ����
 * \param[in]    resource   ͼƬ��Դ����ָ�� 
 * \return       ��
 ************************************************************************************/
void res_manager_free_bitmap_data(void* data);

/************************************************************************************/
/*!
 * \par  Description:
 *     �ͷ����ַ���Դ����
 * \param[in]    resource   �ַ�����Դ����ָ�� 
 * \return       ��
 ************************************************************************************/
void res_manager_free_text_data(void* data);

resource_bitmap_t* res_manager_preload_from_scene(resource_info_t* res_info, resource_scene_t* scene, uint32_t id);
resource_bitmap_t* res_manager_preload_from_group(resource_info_t* res_info, resource_group_t* group, uint32_t id);
resource_bitmap_t* res_manager_preload_from_picregion(resource_info_t* res_info, resource_picregion_t* picreg, uint32_t frame);


#endif /*__RES_MANAGER_API_H__*/
