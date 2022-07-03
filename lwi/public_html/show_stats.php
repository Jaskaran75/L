<?php
    $cookie_id = filter_input( INPUT_COOKIE, session_name( ), FILTER_SANITIZE_FULL_SPECIAL_CHARS );
    if ( ! is_null( $cookie_id ) && is_string( $cookie_id ) ) {
        $session_id = preg_replace( "/[^\da-z]/i", "", $cookie_id );
    } else {
        $session_id = "NOCOOKIE";
    }

    $session_short_id = substr( $session_id, -6 );
?>
<!DOCTYPE html>
<!--
Copyright (C) 2021 Marcelo C. Pereira <mcper at unicamp.br>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
-->
<html>
    <head>
        <title>Statistics</title>
        <meta http-equiv="Content-Security-Policy" content="default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'">
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <link rel="icon" href="favicon.ico">
        <link rel="stylesheet" href="w3.css">
        <link rel="stylesheet" href="lwi.css">
    </head>
    <body>
        <?php require "../load_res.php"; ?>
        <div class='w3-main' style='margin-left:10px; margin-right:10px'>
            <div class='w3-container w3-card-2 w3-margin-bottom' style='margin-top:10px'>
                <h1 class='w3-xxxlarge w3-text-blue'><b>Statistics</b></h1>
                <div class='w3-container' style='margin-top: 30px'>
                    <h2 class='w3-xxlarge w3-text-blue'>Time series descriptive statistics</h2>
                    <table class='w3-table w3-striped w3-white'>
                        <col style='width:22%'>
                        <col style='width:13%'>
                        <col style='width:13%'>
                        <col style='width:13%'>
                        <col style='width:13%'>
                        <col style='width:13%'>
                        <col style='width:13%'>
                        <thead>
                            <td><em>Variable</em></td>
                            <td><em>Mean</em></td>
                            <td><em>Std. Deviation</em></td>
                            <td><em>Minimum</em></td>
                            <td><em>Maximum</em></td>
                            <td><em>Observations</em></td>
                            <td><em>MC Std. Error</em></td>
                        </thead>
                        <?php
                            // insert the table columns data
                            foreach ( $sel_vars as $var ) {
                                if ( ! isset ( $stats[ $var ] ) ) {
                                    continue;
                                }

                                echo "<tr>\n";
                                echo   "<td><b>" . $var . "</b></td>\n";

                                foreach ( $stats[ $var ] as $stat => $val ) {
                                    if ( is_numeric( $val ) && ( $stat == "Obs" || $stats[ $var ][ "Obs" ] != 0 ) ) {
                                        echo "<td>" . sprintf( "%.5G", $val ) . "</td>\n";
                                    } else {
                                        echo "<td>N/A</td>\n";
                                    }
                                }

                                echo "</tr>\n";
                            }
                        ?>
                    </table>
                    <?php
                        // table footnotes
                        $t_msg = "Time step range: from " . ( $first + 1 ) . " to " . $last . ". ";
                        $mc_msg = $mc_runs > 1 ? "Monte Carlo runs: " . $mc_runs . ". " : "";
                        $log_msg = $linear ? "" : "  <em>Log values</em>.";
                        echo "<p>" . $t_msg . $mc_msg . $log_msg . "</p>\n";
                    ?>
                </div>
                <div class="w3-container w3-center" style="margin-top: 30px">
                    <button onclick='window.close( )' class='w3-button w3-blue w3-padding-large w3-margin-right w3-margin-bottom w3-hover-black'>Close</button>
                </div>
            </div>
        </div>
    </body>
</html>
