<?php
// Atomik Web Site - Logout Page 
// HTML/JQUERY/PHP/MySQL
// By Rahim Khoja

// Logs users out of system, waits for specified time period to redirect to login page. Displays custom logout messages

$atomik_heading = $_POST["logout_title"];
$atomik_description = "";
$atomik_description = $_POST["description"];

if ( isset($_POST["count"]) ) {
	$atomik_count = $_POST["count"];
} else {
	$atomik_count = 15;
}

session_start();
unset($_SESSION["username"]);
session_destroy(); // Is Used To Destroy All Sessions
setcookie("atomikAuth", "", time()-3600);
header('Refresh: '.$atomik_count.'; URL = index.php');

?><!doctype html>
<html>
<head>
<meta charset="utf-8">
<title>Atomik Controller - <?php echo $atomik_heading; ?></title>
<link rel="stylesheet" href="css/atomik.css">
</head>
<nav class="navbar navbar-default navbar-inverse">
  <div class="container-fluid"> 
    <!-- Brand and toggle get grouped for better mobile display -->
    <div class="navbar-header">
      <button type="button" class="navbar-toggle collapsed" data-toggle="collapse" data-target="#bs-example-navbar-collapse-1"> <span class="sr-only">Toggle navigation</span> <span class="icon-bar"></span> <span class="icon-bar"></span> <span class="icon-bar"></span> </button>
     </div>
    
    <!-- Collect the nav links, forms, and other content for toggling -->
    <div class="collapse navbar-collapse" id="bs-example-navbar-collapse-1">
      
    </div>
    <!-- /.navbar-collapse --> 
  </div>
  <!-- /.container-fluid --> 
</nav>
<body>
<div class="wrapper">
<div class="PageTitle">
    <div class="row">
        <div class="PageNavTitle" >
        </div>
    </div>
   </div>
<hr>
  <br>
  <div class="container">
        <div class="col-xs-5"><img src="img/Sun_Logo_Center_450px.gif" width="350" height="350" alt=""/></div>
        <div class="col-xs-1"><div style="border-left:1px solid #000;height:350px"></div></div>
        <div class="col-xs-6">
            <h3><p><?php echo $atomik_heading; ?></p></h3>  <br> 
  <h4><?php echo $atomik_description; ?></h4>
  </div>
  <br>
</div>
  <br><hr>
<div class="push"></div>
 </div>
<div class="footer FooterColor">
  
     <hr>
      <div class="col-xs-12 text-center">
        <p>Copyright © Atomik Technologies Inc. All rights reserved.</p>
      </div>
      <hr>
    </div>
</body>
</html>
