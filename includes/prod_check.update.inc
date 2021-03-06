<?php

/**
 * ALL code here is taken from the Backdrop core's update module. prod_check
 * recommends that you turn this module off, so we provide the VERY BASIC
 * functionality to get a list of the installed modules with version info and
 * transfer this to prod_monitor over XMLRPC (alot of data here!) so that
 * prod_monitor can do all the update checking locally.
 * This approach was chosen over this approach:
 *
 *     module_load_include('module', 'update', 'update');
 *
 *     if ($available = update_get_available(TRUE)) {
 *       $data['modules'] = update_calculate_project_data($available);
 *     }
 *
 * This way, we might just as well turn on the update module and periodically
 * download the results. It's just not efficient...
 * Also note that passing TRUE to update_get_available() will bypass (refresh)
 * the caches, essentially the same as we do in _prod_check_module_list().
 * See inline comments there for more info.
 */


/**
 * Taken from Core: modules/update/update.compare.inc, line 81
 *
 * Populate an array of project data.
 *
 * This iterates over a list of the installed modules or themes and groups
 * them by project and status. A few parts of this function assume that
 * enabled modules and themes are always processed first, and if disabled
 * modules or themes are being processed (there is a setting to control if
 * disabled code should be included in the Available updates report or not),
 * those are only processed after $projects has been populated with
 * information about the enabled code. 'Hidden' modules and themes are always
 * ignored. This function also records the latest change time on the .info
 * files for each module or theme, which is important data which is used when
 * deciding if the cached available update data should be invalidated.
 *
 * @param $projects
 *   Reference to the array of project data of what's installed on this site.
 * @param $list
 *   Array of data to process to add the relevant info to the $projects array.
 * @param $project_type
 *   The kind of data in the list (can be 'module' or 'theme').
 * @param $status
 *   Boolean that controls what status (enabled or disabled) to process out of
 *   the $list and add to the $projects array.
 *
 * @see update_get_projects()
 */
function _prod_check_process_info_list(&$projects, $list, $project_type, $status) {
  foreach ($list as $file) {
    // A disabled base theme of an enabled sub-theme still has all of its code
    // run by the sub-theme, so we include it in our "enabled" projects list.
    if ($status && !$file->status && !empty($file->sub_themes)) {
      foreach ($file->sub_themes as $key => $name) {
        // Build a list of enabled sub-themes.
        if ($list[$key]->status) {
          $file->enabled_sub_themes[$key] = $name;
        }
      }
      // If there are no enabled subthemes, we should ignore this base theme
      // for the enabled case. If the site is trying to display disabled
      // themes, we'll catch it then.
      if (empty($file->enabled_sub_themes)) {
        continue;
      }
    }
    // Otherwise, just add projects of the proper status to our list.
    elseif ($file->status != $status) {
      continue;
    }

    // Skip if the .info file is broken.
    if (empty($file->info)) {
      continue;
    }

    // Skip if it's a hidden module or theme.
    if (!empty($file->info['hidden'])) {
      continue;
    }

    // If the .info doesn't define the 'project', try to figure it out.
    if (!isset($file->info['project'])) {
      $file->info['project'] = _prod_check_get_project_name($file);
    }

    // If we still don't know the 'project', give up.
    if (empty($file->info['project'])) {
      continue;
    }

    // If we don't already know it, grab the change time on the .info file
    // itself. Note: we need to use the ctime, not the mtime (modification
    // time) since many (all?) tar implementations will go out of their way to
    // set the mtime on the files it creates to the timestamps recorded in the
    // tarball. We want to see the last time the file was changed on disk,
    // which is left alone by tar and correctly set to the time the .info file
    // was unpacked.
    if (!isset($file->info['_info_file_ctime'])) {
      $info_filename = dirname($file->uri) . '/' . $file->name . '.info';
      $file->info['_info_file_ctime'] = filectime($info_filename);
    }

    if (!isset($file->info['datestamp'])) {
      $file->info['datestamp'] = 0;
    }

    $project_name = $file->info['project'];

    // Figure out what project type we're going to use to display this module
    // or theme. If the project name is 'backdrop', we don't want it to show up
    // under the usual "Modules" section, we put it at a special "Backdrop Core"
    // section at the top of the report.
    if ($project_name == 'Backdrop') {
      $project_display_type = 'core';
    }
    else {
      $project_display_type = $project_type;
    }
    if (empty($status) && empty($file->enabled_sub_themes)) {
      // If we're processing disabled modules or themes, append a suffix.
      // However, we don't do this to a a base theme with enabled
      // subthemes, since we treat that case as if it is enabled.
      $project_display_type .= '-disabled';
    }
    // Add a list of sub-themes that "depend on" the project and a list of base
    // themes that are "required by" the project.
    if ($project_name == 'backdrop') {
      // Backdrop core is always required, so this extra info would be noise.
      $sub_themes = array();
      $base_themes = array();
    }
    else {
      // Add list of enabled sub-themes.
      $sub_themes = !empty($file->enabled_sub_themes) ? $file->enabled_sub_themes : array();
      // Add list of base themes.
      $base_themes = !empty($file->base_themes) ? $file->base_themes : array();
    }
    if (!isset($projects[$project_name])) {
      // Only process this if we haven't done this project, since a single
      // project can have multiple modules or themes.
      $projects[$project_name] = array(
        'name' => $project_name,
        // Only save attributes from the .info file we care about so we do not
        // bloat our RAM usage needlessly.
        'info' => _prod_check_filter_project_info($file->info),
        'datestamp' => $file->info['datestamp'],
        'includes' => array($file->name => $file->info['name']),
        'project_type' => $project_display_type,
        'project_status' => $status,
        'sub_themes' => $sub_themes,
        'base_themes' => $base_themes,
      );
    }
    elseif ($projects[$project_name]['project_type'] == $project_display_type) {
      // Only add the file we're processing to the 'includes' array for this
      // project if it is of the same type and status (which is encoded in the
      // $project_display_type). This prevents listing all the disabled
      // modules included with an enabled project if we happen to be checking
      // for disabled modules, too.
      $projects[$project_name]['includes'][$file->name] = $file->info['name'];
      $projects[$project_name]['info']['_info_file_ctime'] = max($projects[$project_name]['info']['_info_file_ctime'], $file->info['_info_file_ctime']);
      $projects[$project_name]['datestamp'] = max($projects[$project_name]['datestamp'], $file->info['datestamp']);
      if (!empty($sub_themes)) {
        $projects[$project_name]['sub_themes'] += $sub_themes;
      }
      if (!empty($base_themes)) {
        $projects[$project_name]['base_themes'] += $base_themes;
      }
    }
    elseif (empty($status)) {
      // If we have a project_name that matches, but the project_display_type
      // does not, it means we're processing a disabled module or theme that
      // belongs to a project that has some enabled code. In this case, we add
      // the disabled thing into a separate array for separate display.
      $projects[$project_name]['disabled'][$file->name] = $file->info['name'];
    }
  }
}


/**
 * Taken from Core: modules/update/update.compare.inc, line 238
 *
 * Given a $file object (as returned by system_get_files_database()), figure
 * out what project it belongs to.
 *
 * @see system_get_files_database()
 */
function _prod_check_get_project_name($file) {
  $project_name = '';
  if (isset($file->info['project'])) {
    $project_name = $file->info['project'];
  }
  elseif (isset($file->info['package']) && (strpos($file->info['package'], 'Core') === 0)) {
    $project_name = 'backdrop';
  }
  return $project_name;
}

/**
 * Taken from Core: modules/update/update.compare.inc, line 684
 *
 * Filter the project .info data to only save attributes we need.
 *
 * @param array $info
 *   Array of .info file data as returned by backdrop_parse_info_file().
 *
 * @return
 *   Array of .info file data we need for the Update manager.
 *
 * @see _prod_check_process_info_list()
 */
function _prod_check_filter_project_info($info) {
  $whitelist = array(
    '_info_file_ctime',
    'datestamp',
    'major',
    'name',
    'package',
    'project',
    'project status url',
    'version',
  );
  return array_intersect_key($info, backdrop_map_assoc($whitelist));
}
