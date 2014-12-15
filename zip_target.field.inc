<?php

/**
 * @file
 * Field module functionality for the Zip Upload Extract Widget module.
 */

/**
 * Implements hook_field_info().
 */

/** 
	General Field definition 
	Includes field and all subvalues to be stored.  This includes defaults on field definition form and values on data entry form.
**/

function zip_target_field_info() {
  return array(
    'zip_target' => array(
      'label' => t('Zip Upload File'),
      'description' => t('This field stores the ID of a file as an integer value.'),
      'settings' => array(
        'uri_scheme' => variable_get('file_default_scheme', 'public'),
      ),
      'instance_settings' => array(
		'link_text' => '',
        'file_extensions' => 'zip',
        'file_directory' => '',
		'target_file' => '',
		'max_filesize' => '',
      ),
      'default_widget' => 'zip_target_generic',
      'default_formatter' => 'zip_target_formatter_link_target',
    ),

  );
}


/**
 * Implements hook_field_settings_form().
 */

/*
	Field Definition Form Settings for everywhere field appears - we don't really have any except for public vs. private
*/

function zip_target_field_settings_form($field, $instance, $has_data) {
  $defaults = field_info_field_settings($field['type']);
  $settings = array_merge($defaults, $field['settings']);

  //$form['#attached']['js'][] = drupal_get_path('module', 'zip_target') . '/zip_target.js';

  //$form['#element_validate'][] = array('zip_target_field_settings_form_validate');

  $scheme_options = array();

  foreach (file_get_stream_wrappers(STREAM_WRAPPERS_WRITE_VISIBLE) as $scheme => $stream_wrapper) {
    $scheme_options[$scheme] = $stream_wrapper['name'];
  }
  $form['uri_scheme'] = array(
    '#type' => 'radios',
    '#title' => t('Upload destination'),
    '#options' => $scheme_options,
    '#default_value' => $settings['uri_scheme'],
    '#description' => t('Select where the final files should be stored. Private file storage has significantly more overhead than public files, but allows restricted access to files within this field.'),
    '#disabled' => $has_data,
  );

  return $form;
}



/**
 * Implements hook_field_instance_settings_form().
 */
 
 /** Basically the field definition form **/

function zip_target_field_instance_settings_form($field, $instance) {
  $settings = $instance['settings'];

  $form['file_directory'] = array(
    '#type' => 'textfield',
    '#title' => t('Zip File directory'),
    '#default_value' => $settings['file_directory'],
    '#description' => t('Optional subdirectory within the upload destination where zip files will be stored. Do not include preceding or trailing slashes.'),
    '#element_validate' => array('_zip_target_generic_settings_file_directory_validate'),
    '#weight' => 3,
  );


  $form['target_file'] = array(
    '#type' => 'textfield',
    '#title' => t('Target file within the Zip archive'),
    '#default_value' => $settings['target_file'],
    '#description' => t('Target File within the Zip archive (typically an html file, e.g. index.html) to be used as link.'),
    '#element_validate' => array('_zip_target_generic_settings_target_file_validate'),
    '#weight' => 4,
  );


  $form['max_filesize'] = array(
    '#type' => 'textfield',
    '#title' => t('Maximum upload size'),
    '#default_value' => $settings['max_filesize'],
    '#description' => t('Enter a value like "512" (bytes), "80 KB" (kilobytes) or "50 MB" (megabytes) in order to restrict the allowed file size. If left empty the file sizes will be limited only by PHP\'s maximum post and file upload sizes (current limit <strong>%limit</strong>).', array('%limit' => format_size(file_upload_max_size()))),
    '#size' => 10,
    '#element_validate' => array('_zip_target_generic_settings_max_filesize'),
    '#weight' => 5,
  );

  return $form;
}


/**
 * Element validate callback for the maximum upload size field.
 *
 * Ensure a size that can be parsed by parse_size() has been entered.
 */

/** This is validation of values entered on field definition form **/

function _zip_target_generic_settings_max_filesize($element, &$form_state) {
  if (!empty($element['#value']) && !is_numeric(parse_size($element['#value']))) {
    form_error($element, t('The "!name" option must contain a valid value. You may either leave the text field empty or enter a string like "512" (bytes), "80 KB" (kilobytes) or "50 MB" (megabytes).', array('!name' => t($element['title']))));
  }
}


/**
 * Element validate callback for the allowed file extensions field.
 *
 * This doubles as a convenience clean-up function and a validation routine.
 * Commas are allowed by the end-user, but ultimately the value will be stored
 * as a space-separated list for compatibility with file_validate_extensions().
 */

/** This is validation of values entered on field definition form **/

function _zip_target_generic_settings_extensions($element, &$form_state) {
  if (!empty($element['#value'])) {
    $extensions = preg_replace('/([, ]+\.?)/', ' ', trim(strtolower($element['#value'])));
    $extensions = array_filter(explode(' ', $extensions));
    $extensions = implode(' ', array_unique($extensions));
    if (!preg_match('/^([a-z0-9]+([.][a-z0-9])* ?)+$/', $extensions)) {
      form_error($element, t('The list of allowed extensions is not valid, be sure to exclude leading dots and to separate extensions with a comma or space.'));
    }
    else {
      form_set_value($element, $extensions, $form_state);
    }
  }
}


/**
 * Element validate callback for the zip file destination field.
 *
 * Remove slashes from the beginning and end of the destination value and ensure
 * that the file directory path is not included at the beginning of the value.
 */

/** This is validation of values entered on field definition form **/

function _zip_target_generic_settings_file_directory_validate($element, &$form_state) {
  // Strip slashes from the beginning and end of $widget['file_directory'].
  $value = trim($element['#value'], '\\/');
  form_set_value($element, $value, $form_state);
}


/**
 * Element validate callback for the zip file target file field.
 *
 * Remove slashes from the beginning and end of the target file value and ensure
 * that the file directory path is not included at the beginning of the value.
 */

/** This is validation of values entered on field definition form **/

function _zip_target_generic_settings_target_file_validate($element, &$form_state) {
  // Strip slashes from the beginning and end of $widget['target_file'].
  $value = trim($element['#value'], '\\/');
  form_set_value($element, $value, $form_state);
}



/**
 * Implements hook_field_load().
 */

/** Loading values on the data entry form?  **/

function zip_target_field_load($entity_type, $entities, $field, $instances, $langcode, &$items, $age) {

  $fids = array();
  foreach ($entities as $id => $entity) {
    // Load the files from the files table.
    foreach ($items[$id] as $delta => $item) {
      if (!empty($item['fid'])) {
        $fids[] = $item['fid'];
      }
    }
  }
  $files = file_load_multiple($fids);

  foreach ($entities as $id => $entity) {
    foreach ($items[$id] as $delta => $item) {
      // If the file does not exist, mark the entire item as empty.
      if (empty($item['fid']) || !isset($files[$item['fid']])) {
        $items[$id][$delta] = NULL;
      }
      else {
        $items[$id][$delta] = array_merge($item, (array) $files[$item['fid']]);
      }
    }
  }
}




/**
 * Implements hook_field_presave().
 */

function zip_target_field_presave($entity_type, $entity, $field, $instance, $langcode, &$items) {

  // Make sure that each file which will be saved with this object has a
  // permanent status, so that it will not be removed when temporary files are
  // cleaned up.
  foreach ($items as $item) {
    $file = file_load($item['fid']);
    if (!$file->status) {
      $file->status = FILE_STATUS_PERMANENT;
      file_save($file);
    }
  }
}


/**
 * Implements hook_field_insert().
 */
function zip_target_field_insert($entity_type, $entity, $field, $instance, $langcode, &$items) {
  file_field_insert($entity_type, $entity, $field, $instance, $langcode, $items);
}



/**
 * Implements hook_field_update().
 *
 * Checks for files that have been removed from the object.
 */
function zip_target_field_update($entity_type, $entity, $field, $instance, $langcode, &$items) {

	if(empty($entity->revision)) {

		// Get all FIDs of the submitted field
		$current_fids = array();
		foreach ($items as $item) {
			$current_fids[] = $item['fid'];
		}

		// if there are no revisions
		// remove the unzipped folder as it's not needed anymore
		if (isset($entity->original)) {
			$original = $entity->original;
			if (!empty($original->{$field['field_name']}[$langcode])) {
				foreach ($original->{$field['field_name']}[$langcode] as $original_item) {
					if (isset($original_item['fid']) && !in_array($original_item['fid'], $current_fids)) {
						// delete the unzipped path that's not needed anymore
						$drupal_uri = $field['settings']['uri_scheme'] .'://'. trim($instance['settings']['file_directory'], '/') .'/'.$original_item['unzipped_folder'];
						file_unmanaged_delete_recursive($drupal_uri);
					}
				}
			}
		}
	}

	file_field_update($entity_type, $entity, $field, $instance, $langcode, $items);
}


/**
 * Implements hook_field_delete().
 */
function zip_target_field_delete($entity_type, $entity, $field, $instance, $langcode, &$items) {
	// Removing zip upload extract field from a content type

	// delete any unzipped file
	foreach ($items as $item) {
		if (!empty($item['fid'])) {
			$drupal_uri = $field['settings']['uri_scheme'] .'://'. trim($instance['settings']['file_directory'], '/') .'/'.$item['unzipped_folder'];
			file_unmanaged_delete_recursive($drupal_uri);
		}
	}

	file_field_delete($entity_type, $entity, $field, $instance, $langcode, $items);

}


/**
 * Implements hook_delete_revision().
 */
function zip_target_field_delete_revision($entity_type, $entity, $field, $instance, $langcode, &$items) {
	$storeitems = $items;
	file_field_delete_revision($entity_type, $entity, $field, $instance, $langcode, $items);
	foreach ($storeitems as $delta => $item) {
		$file = (object) $item;
		$file_usage = file_usage_list($file);

		if (empty($file_usage['file'])) {
			//file_usage_delete($file, 'file', $entity_type, $id, $count);
			$drupal_uri = $field['settings']['uri_scheme'] .'://'. trim($instance['settings']['file_directory'], '/') .'/'.$item['unzipped_folder'];
			file_unmanaged_delete_recursive($drupal_uri);
		}
	}
}




/**
 * Implements hook_field_is_empty().
 */
function zip_target_field_is_empty($item, $field) {
  return empty($item['fid']);
}



/**
 * Implements hook_field_widget_info().
 */
function zip_target_field_widget_info() {
  return array(
    'zip_target_generic' => array(
      'label' => t('Zip File'),
      'field types' => array('zip_target'), // what is this referencing
      'settings' => array(
        'progress_indicator' => 'throbber',
      ),
      'behaviors' => array(
        'multiple values' => FIELD_BEHAVIOR_CUSTOM,
        'default value' => FIELD_BEHAVIOR_NONE,
      ),
    ),

  );
}


/**
 * Implements hook_field_widget_settings_form().
 */

/** Widget/field settings for the data entry form **/

function zip_target_field_widget_settings_form($field, $instance) {
  $widget = $instance['widget'];
  $settings = $widget['settings'];

  $form['progress_indicator'] = array(
    '#type' => 'radios',
    '#title' => t('Progress indicator'),
    '#options' => array(
      'throbber' => t('Throbber'),
      'bar' => t('Bar with progress meter'),
    ),
    '#default_value' => $settings['progress_indicator'],
    '#description' => t('The throbber display does not show the status of uploads but takes up less space. The progress bar is helpful for monitoring progress on large uploads.'),
    '#weight' => 16,
    '#access' => file_progress_implementation(),
  );

  return $form;
}



/**
 * Implements hook_field_widget_form().
 */

/*	This is completely different */

function zip_target_field_widget_form(&$form, &$form_state, $field, $instance, $langcode, $items, $delta, $element) {

/** Widget on the data entry screen **/

  // Add display_field setting to field because file_field_widget_form() assumes it is set.
  $field['settings']['display_field'] = 0;

  $elements = file_field_widget_form($form, $form_state, $field, $instance, $langcode, $items, $delta, $element);
  $settings = $instance['settings'];


  foreach (element_children($elements) as $delta) {
    // Add upload resolution validation.

    // If not using custom extension validation, ensure this is an zip file.
    $supported_extensions = array('zip');
    $extensions = isset($elements[$delta]['#upload_validators']['file_validate_extensions'][0]) ? $elements[$delta]['#upload_validators']['file_validate_extensions'][0] : implode(' ', $supported_extensions);
    $extensions = array_intersect(explode(' ', $extensions), $supported_extensions);
    $elements[$delta]['#upload_validators']['file_validate_extensions'][0] = implode(' ', $extensions);
	$elements[$delta]['#upload_validators']['zip_target_validate_file'][0] = $instance['settings']['target_file'];

    // Add all extra functionality provided by the image widget.
    $elements[$delta]['#process'][] = 'zip_target_field_widget_process';
  }

  if ($field['cardinality'] == 1) {
    // If there's only one field, return it as delta 0.
    if (empty($elements[0]['#default_value']['fid'])) {
      $elements[0]['#description'] = theme('file_upload_help', array('description' => $instance['description'], 'upload_validators' => $elements[0]['#upload_validators']));
    }
  }
  else {
    $elements['#file_upload_description'] = theme('file_upload_help', array('upload_validators' => $elements[0]['#upload_validators']));
  }


  return $elements;
}

/**
 **
 **	Static object with deails about our zip file
 ** including the Zip::Archive instantiation handler
 **
 **/

function zip_target_zip_handler($reset=FALSE, $key=NULL, $value=NULL) {
	static $zip;
	if ($reset || empty($zip)) {
		$zip = new stdClass();
		$zip->obj = new ZipArchive;	
	}

	if (!empty($key) && !empty($value))  $zip->$key = $value;
	return $zip;
}


function zip_target_validate_file(stdClass $file, $target_file) {

	$errors = array();

	$zip = new ZipArchive;
	if ($zip->open($file->uri)) {
		$ziplist = _zip_target_get_zip_list($zip);
		$targetlist = array();
		foreach ($ziplist as $z_key => $z_val) {
			if (strripos($z_val, $target_file)) $targetlist[]=$z_val;
		}
		if (empty($targetlist)) {
		    $errors[] = t('Unable to find target file in zip archive.');
		} else if (count($targetlist) > 1) {
			$errors[] = t('Multiple target files were found in the zip archive.');
		}
		$zip->close();
		//$zip = zip_target_zip_handler(FALSE,'target',$targetlist[0]);
	} else {
		$errors[] = t('Unable to open the zip archive.');
	}

	return $errors;
}



/**
 * An element #process callback for the file_generic field type.
 *
 * Expands the file_generic type to include the description and display fields.
 */
function zip_target_field_widget_process($element, &$form_state, $form) {

  $item = $element['#value'];
  $item['fid'] = $element['fid']['#value'];

  $field = field_widget_field($element, $form_state);
  $instance = field_widget_instance($element, $form_state);
  $settings = $instance['widget']['settings'];

  $element['#theme'] = 'zip_target_widget';
  $element['#attached']['css'][] = drupal_get_path('module', 'zip-target') . '/zip-target.css';

	if (!empty($element['#file']->uri)) {
		
		if ($element['#default_value']['fid'] == 0) {
			preg_match('/.*\/(.*)\.zip$/i', $element['#file']->uri, $matches);
			//$upload_location = drupal_realpath($element['#upload_location'].'/'.$matches[1]);

			$unzippedtarget = _zip_target_unpack_file(drupal_realpath($element['#file']->uri), 
															  drupal_realpath($element['#upload_location']).'/'.$matches[1], 
															  $instance['settings']['target_file'], $matches[1]);

			$item['unzipped_folder'] = $unzippedtarget['unzipped_folder'];
			$item['target_file'] = $unzippedtarget['target_file'];

		}

		  $element['link_text'] = array(
			'#title' => t('Link Text'),
			'#type' => 'textfield',
			'#default_value' => isset($item['link_text']) ? $item['link_text'] : '',
			'#description' => t('Text to be used for the link to the Target File'),
			'#maxlength' => 512,
			'#weight' => 2,
			//'#access' => (bool) $item['fid'] && $settings['target_file'],
		  );

		  $element['unzipped_folder'] = array(
			'#title' => t('Unzipped Folder'),
			'#type' => 'textfield',
			'#default_value' => isset($item['unzipped_folder']) ? $item['unzipped_folder'] : '',
			//'#default_value' => $unzippedtarget['unzipped_folder'],
			'#description' => t('The path of the unzipped file.'),
			'#maxlength' => 512,
			'#weight' => 4,
			//'#access' => (bool) $item['fid'] && $settings['unzipped_folder'],
		  );

		  $element['target_file'] = array(
			'#title' => t('Target File'),
			'#type' => 'textfield',
			'#default_value' => isset($item['target_file']) ? $item['target_file'] : '',
			//'#default_value' => $unzippedtarget['target_file'],
			'#description' => t('The unzipped file to be returned for use'),
			'#maxlength' => 512,
			'#weight' => 6,
			//'#access' => (bool) $item['fid'] && $settings['target_file'],
		  );

	}


  return $element;
}



/**
 * Returns HTML for an individual file upload widget.
 *
 * @param $variables
 *   An associative array containing:
 *   - element: A render element representing the widget.
 *
 * @ingroup themeable
 */

function theme_zip_target_widget($variables) {
	$element = $variables['element'];
	$output = '';
	$output .= '<div class="zip-target-widget form-managed-file clearfix">';

	$output .= '<div class="zip-target-widget-data">';
	if ($element['fid']['#value'] != 0) {
	$element['filename']['#markup'] .= ' <span class="file-size">(' . format_size($element['#file']->filesize) . ')</span> ';
	}
	$output .= drupal_render_children($element);
	$output .= '</div>';
	$output .= '</div>';

	return $output;
}



/**
 **	Implements hook_field_validate()
 **/

function zip_target_field_validate($entity_type, $entity, $field, $instance, $langcode, $items, &$errors) {

	foreach ($items as $delta => $item) {
		if (!empty($item['unzipped_folder'])) {
			switch ($item['unzipped_folder']) {
				case 'zip-target-file-not-found':
					$errors[$field['field_name']][$langcode][$delta][] = array(
					  'error' => 'zip-target-file-not-found',
					  'message' => t('%title: Unable to find target file in zip archive.', array('%title' => $instance['label'])));
					break;
				case 'zip-target-multiple-targets':
					$errors[$field['field_name']][$langcode][$delta][] = array(
					  'error' => 'zip-target-multiple-targets',
					  'message' => t('%title: Multiple target files were found in the zip archive.', array('%title' => $instance['label'])));
					break;
				case 'zip-target-unable-to-unzip':
					$errors[$field['field_name']][$langcode][$delta][] = array(
					  'error' => 'zip-target-unable-to-unzip',
					  'message' => t('%title: Unable to unzip archive.  Verify the archive is correct and check directory permissions.', array('%title' => $instance['label'])));
					break;
				case 'zip-target-unable-to-open-zip':
					$errors[$field['field_name']][$langcode][$delta][] = array(
					  'error' => 'zip-target-unable-to-open-zip',
					  'message' => t('%title: Unable to open the zip archive.', array('%title' => $instance['label'])));
					break;
			}
		}
	}

}



function _zip_target_unpack_file($zipfile, $zippath, $target_file, $zipadjust) {

	$zip = new ZipArchive;
	if ($zip->open($zipfile)) {
		$ziplist = _zip_target_get_zip_list($zip);
		$targetlist = array();
		$unzipresult['error'] = FALSE;
		foreach ($ziplist as $z_key => $z_val) {
			if (strripos($z_val, $target_file)) $targetlist[]=$z_val;
		}
		if (empty($targetlist)) {
			$unzipresult['error'] = 'zip-target-file-not-found';
		} else if (count($targetlist) > 1) {
			$unzipresult['error'] = 'zip-target-multiple-targets';
		} else {
			$unzipresult['target_file'] = $zipadjust.'/'.$targetlist[0];
			//$unzipresult['unzipped_folder'] = substr($targetlist[0],0,strpos($targetlist[0],'/'));
			$unzipresult['unzipped_folder'] = $zipadjust;
			if (!$zip->extractTo($zippath)) {
				$unzipresult['error'] = 'zip-target-unable-to-unzip';
			}
		}
		$zip->close();
	} else {
		$unzipresult['error'] = 'zip-target-unable-to-open-zip';
	}
	if (!empty($unzipresult['error'])) {
		$unzipresult['target_file'] = $unzipresult['error'];
		$unzipresult['unzipped_folder'] = $unzipresult['error'];
	}

	return $unzipresult;
}



function _zip_target_get_zip_list($zip) {
	$ziplist = array();
	for($i = 0; $i < $zip->numFiles; $i++){ 
		$stat = $zip->statIndex($i); 
		$ziplist[] = $stat['name']; 
	}
	return $ziplist;
}


/** Display Functions **/



/**
 * Implements hook_field_prepare_view().
 */

/** Format field for display **/

function zip_target_field_prepare_view($entity_type, $entities, $field, $instances, $langcode, &$items) {
  // Remove files specified to not be displayed.
  foreach ($entities as $id => $entity) {
    foreach ($items[$id] as $delta => $item) {
      if (!zip_target_field_displayed($item, $field)) {
        unset($items[$id][$delta]);
      }
    }
    // Ensure consecutive deltas.
    $items[$id] = array_values($items[$id]);
  }
}



/**
 * Determines whether a file should be displayed when outputting field content.
 *
 * @param $item
 *   A field item array.
 * @param $field
 *   A field array.
 *
 * @return
 *   Boolean TRUE if the file will be displayed, FALSE if the file is hidden.
 */
function zip_target_field_displayed($item, $field) {
  if (!empty($field['settings']['display_field'])) {
    return (bool) $item['display'];
  }
  return TRUE;
}

/**
 * Implements hook_field_formatter_info().
 */
function zip_target_field_formatter_info() {
  $formatters = array(
    'zip_target_formatter_link_target' => array(
      'label' => t('Link to Target File'),
      'field types' => array('zip_target'),
    ),
  
    'zip_target_formatter_url_target' => array(
      'label' => t('URL to Target File'),
      'field types' => array('zip_target'),
    ),

    'zip_target_formatter_path_target' => array(
      'label' => t('Path to Target File'),
      'field types' => array('zip_target'),
    ),

  );

  return $formatters;
}


/**
 * Implements hook_field_formatter_view().
 */
function zip_target_field_formatter_view($entity_type, $entity, $field, $instance, $langcode, $items, $display) {
	
	$element = array();

	switch ($display['type']) {
	case 'zip_target_formatter_url_target':
		$format_field = 'zip_target_zip_target_url_target_formatter';
	break;

	case 'zip_target_formatter_path_target':
		$format_field = 'zip_target_zip_target_url_target_formatter';
	break;

	default:
		$format_field = 'zip_target_zip_target_link_target_formatter';
	}

	foreach ($items as $delta => $item) {
		$element[$delta] = $format_field($item, $field, $instance);
	}

  return $element;

}


/**
 * Returns HTML for a link to target field formatter.
 *
 * @param $variables
 *   An associative array containing:
 *   - item: Associative array of image data, which may include "uri", "alt",
 *     "width", "height", "title" and "attributes".
 *   - image_style: An optional image style.
 *   - path: An array containing the link 'path' and link 'options'.
 *
 * @ingroup themeable
 */
function zip_target_zip_target_link_target_formatter($item, &$field, &$instance) {

//$field['settings']['uri_scheme']
//$instance['settings']['file_directory']


	$drupal_uri = $field['settings']['uri_scheme'] .'://'. trim($instance['settings']['file_directory'], '/') .'/'.$item['target_file'];

	if (empty($item['link_text'])) {
		$link_text = file_create_url($drupal_uri);
	}
	else {
		$link_text = $item['link_text'];
		//$options['attributes']['title'] = check_plain($file->filename);
	}
	$object['#markup'] = '<span class="url">' . l($link_text, file_create_url($drupal_uri), array('attributes' => array('target'=>'_blank'))) . '</span>';

	return $object;
}



/**
 * Returns HTML for a url to target field formatter.
 *
 * @param $variables
 *   An associative array containing:
 *   - item: Associative array of image data, which may include "uri", "alt",
 *     "width", "height", "title" and "attributes".
 *   - image_style: An optional image style.
 *   - path: An array containing the link 'path' and link 'options'.
 *
 * @ingroup themeable
 */
function zip_target_zip_target_url_target_formatter($item, &$field, &$instance) {
	$drupal_uri = $field['settings']['uri_scheme'] .'://'. trim($instance['settings']['file_directory'], '/') .'/'.$item['target_file'];
	$object['#markup'] = check_plain(file_create_url($drupal_uri));
	return $object;
}


/**
 * Returns HTML for a path to target field formatter.
 *
 * @param $variables
 *   An associative array containing:
 *   - item: Associative array of image data, which may include "uri", "alt",
 *     "width", "height", "title" and "attributes".
 *   - image_style: An optional image style.
 *   - path: An array containing the link 'path' and link 'options'.
 *
 * @ingroup themeable
 */
function zip_target_zip_target_path_target_formatter($item, &$field, &$instance) {
	$drupal_uri = $field['settings']['uri_scheme'] .'://'. trim($instance['settings']['file_directory'], '/') .'/'.$item['target_file'];
	$object['#markup'] = check_plain(drupal_realpath($drupal_uri));
	return $object;
}


