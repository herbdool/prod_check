<?php
// $Id$

/**
 * Implementation of hook_drush_command().
 */
function prod_check_drush_command() {
  $items = array();

  $items['prod-check'] = array(
    'callback' => 'prod_check_status',
    'description' => "Display the Production Check status page",
  );
  
  return $items;
}

/**
 * Status page callback
 */
function prod_check_status() {
  $rows = array(array("\033[1m".dt('Production Check status')."\033[0m"));
  // Map error codes to shell colours.
  $severity = array (
    REQUIREMENT_INFO => '1',
    REQUIREMENT_OK => '1;32',
    REQUIREMENT_WARNING => '1;33',
    REQUIREMENT_ERROR => '1;31',
  );
  $error = 0;

  foreach (_prod_check_functions() as $set => $data) {
    $rows[] = array('');
    $rows[] = array("\033[1m".dt($data['title'])."\033[0m");
    foreach ($data['functions'] as $function => $title) {
      $func = '_prod_check_' . $function;
      // $func() will execute the function with the name we composed above.
      $result = $func();
      $func = ltrim($func, '_');
      if (!empty($result)) {
        $rows[] = array(
          $result[$func]['title'],
          "\033[".$severity[$result[$func]['severity']].'m'.strip_tags($result[$func]['value'])."\033[0m",
        );
        if ($error < $result[$func]['severity']) {
          $error = $result[$func]['severity'];
        }
      }
    }
  }
  drush_print_table($rows, TRUE);
  if ($error > 0) {
    // Would be cool if we could prefix the admin path with http://<host>/ so it
    // will become a clickable link in some terminals. Any deas?
    drush_print(dt("\033[1m".'Some errors were reported!'."\033[0m".' Check the full status page on '."\033[1m".'admin/reports/prod-check'."\033[0m".' for details.'));
  }
}