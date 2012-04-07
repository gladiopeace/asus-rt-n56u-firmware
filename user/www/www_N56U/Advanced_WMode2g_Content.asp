<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>
<head>
<meta http-equiv="X-UA-Compatible" content="IE=EmulateIE7"/>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title>ASUS Wireless Router <#Web_Title#> - <#menu5_1_3#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">

<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script type="text/javascript" language="JavaScript" src="/help_2g.js"></script>
<script language="JavaScript" type="text/javascript" src="/general_2g.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script language="JavaScript" type="text/javascript" src="/detect.js"></script>
<script>
wan_route_x = '<% nvram_get_x("IPConnection", "wan_route_x"); %>';
wan_nat_x = '<% nvram_get_x("IPConnection", "wan_nat_x"); %>';
wan_proto = '<% nvram_get_x("Layer3Forwarding",  "wan_proto"); %>';

<% login_state_hook(); %>
var wireless = [<% wl_auth_list(); %>];	// [[MAC, associated, authorized], ...]

function initial(){
	show_banner(1);
	if(sw_mode == "1" || sw_mode == "4")
		show_menu(5,1,3);
	else
		show_menu(5,1,2);		

	document.form.rt_channel.value = document.form.rt_channel_orig.value;	
	
	show_footer();
	enable_auto_hint(1, 3);
	load_body();
	insertExtChannelOption_wmode();
}

function applyRule(){
	if(document.form.rt_mode_x.value == "0"){
		inputRCtrl1(document.form.rt_wdsapply_x, 1);
		inputRCtrl2(document.form.rt_wdsapply_x, 1);
	}
	
	if(document.form.rt_mode_x.value == "1")
		document.form.rt_wdsapply_x.value = "1";

	showLoading();	
	document.form.action_mode.value = " Restart ";
	document.form.current_page.value = "/as.asp";
	document.form.next_page.value = "Advanced_WMode2g_Content.asp";
	document.form.submit();
}

function done_validating(action){
	refreshpage();
}

function insertExtChannelOption_wmode()
{
var wmode = document.form.rt_gmode.value;
var CurrentCh = document.form.rt_channel.value;
var option_length = document.form.rt_channel.options.length;
if ((wmode == "2"||wmode == "3") && document.form.rt_HT_BW.value == "1")
{/*inputCtrl(document.form.rt_HT_EXTCHA, 1);*/
var x = document.form.rt_HT_EXTCHA;
var length = document.form.rt_HT_EXTCHA.options.length;
if (length > 1)
{x.selectedIndex = 1;
x.remove(x.selectedIndex);
}
if ((CurrentCh >=1) && (CurrentCh <= 4))
{x.options[0].text = 1*CurrentCh + 4;
x.options[0].value = 1;
}
else if ((CurrentCh >= 5) && (CurrentCh <= 7))
{x.options[0].text = 1*CurrentCh - 4;
x.options[0].value = 0;
CurrentCh = 1*CurrentCh;
CurrentCh += 4;
add_a_option(document.form.rt_HT_EXTCHA, 1, CurrentCh);
if (document.form.rt_HT_EXTCHA_old.value == 1)
document.form.rt_HT_EXTCHA.options.selectedIndex=1;
}
else if ((CurrentCh >= 8) && (CurrentCh <= 9))
{x.options[0].text = 1*CurrentCh - 4;
x.options[0].value = 0;
if (option_length >=14)
{CurrentCh = 1*CurrentCh;
CurrentCh += 4;
add_a_option(document.form.rt_HT_EXTCHA, 1, CurrentCh);
if (document.form.rt_HT_EXTCHA_old.value == 1)
document.form.rt_HT_EXTCHA.options.selectedIndex=1;
}
}
else if (CurrentCh == 10)
{x.options[0].text = 1*CurrentCh - 4;
x.options[0].value = 0;
if (option_length > 14)
{CurrentCh = 1*CurrentCh;
CurrentCh += 4;
add_a_option(document.form.rt_HT_EXTCHA, 1, CurrentCh);
if (document.form.rt_HT_EXTCHA_old.value == 1)
document.form.rt_HT_EXTCHA.options.selectedIndex=1;
}
}
else if (CurrentCh >= 11)
{x.options[0].text = 1*CurrentCh - 4;
x.options[0].value = 1;
}
else
{x.options[0].text = "Auto";
x.options[0].value = "1";
}
}
/*else
inputCtrl(document.form.rt_HT_EXTCHA, 0);*/
}
</script>
</head>

<body onload="initial();" onunLoad="disable_auto_hint(1, 3);return unload_body();">
<div id="TopBanner"></div>

<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>
<form method="post" name="form" id="ruleForm" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" value="<% nvram_get_f("general.log","productid"); %>" name="productid" >
<input type="hidden" value="<% nvram_get_x("IPConnection","wan_route_x"); %>" name="wan_route_x" >
<input type="hidden" value="<% nvram_get_x("IPConnection","wan_nat_x"); %>" name="wan_nat_x" >

<input type="hidden" name="current_page" value="Advanced_WMode2g_Content.asp">
<input type="hidden" name="next_page" value="Advanced_WMode2g_Content.asp">
<input type="hidden" name="next_host" value="">
<input type="hidden" name="sid_list" value="WLANConfig11a;WLANConfig11b;">
<input type="hidden" name="group_id" value="rt_RBRList">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="">
<input type="hidden" name="first_time" value="">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get_x("LANGUAGE", "preferred_lang"); %>">
<input type="hidden" name="rt_ssid2" value="<% nvram_get_x("WLANConfig11b",  "rt_ssid2"); %>">
<input type="hidden" name="firmver" value="<% nvram_get_x("",  "firmver"); %>">

<input type="hidden" maxlength="15" size="15" name="x_RegulatoryDomain" value="<% nvram_get_x("Regulatory","x_RegulatoryDomain"); %>" readonly="1">
<input type="hidden" name="rt_wdsnum_x_0" value="<% nvram_get_x("WLANConfig11b", "rt_wdsnum_x"); %>" readonly="1">  
<input type="hidden" name="rt_channel_orig" value='<% nvram_get_x("WLANConfig11b","rt_channel"); %>'>
<input type="hidden" name="rt_gmode" value='<% nvram_get_x("WLANConfig11b","rt_gmode"); %>'>
<input type="hidden" name="rt_HT_EXTCHA" value='<% nvram_get_x("WLANConfig11b","rt_HT_EXTCHA"); %>'>
<input type="hidden" name="rt_HT_BW" value='<% nvram_get_x("WLANConfig11b","rt_HT_BW"); %>'>
      </th>

<table class="content" align="center" cellpadding="0" cellspacing="0">
	<tr>
		<td width="23">&nbsp;</td>
		
		<td valign="top" width="202">				
		<div id="mainMenu"></div>	
		<div id="subMenu"></div>		
		</td>				
		
    	<td valign="top">
		<div id="tabMenu" class="submenuBlock"></div>
		
		<br />
		<!--===================================Beginning of Main Content===========================================-->
<table width="98%" border="0" align="center" cellpadding="0" cellspacing="0">
	<tr>
		<td align="left" valign="top" >
		
<table width="500" border="0" align="center" cellpadding="5" cellspacing="0" class="FormTitle">
	<thead>
	<tr>
		<td><#menu5_1#> - <#menu5_1_3#> (2.4GHz)</td>
	</tr>
	</thead>	
	<tbody>
	<tr>
		<td bgcolor="#FFFFFF"><#WLANConfig11b_display3_sectiondesc#>
			<ul>
				<li><#WLANConfig11b_display3_sectiondesc1#></li>
				<li><#WLANConfig11b_display3_sectiondesc2#></li>
				<li><#WLANConfig11b_display3_sectiondesc3#></li>
				<li>RT-N56U's 2.4GHz MAC address is [<% nvram_get_x("", "il1macaddr"); %>]</li>
			<ul/>
		</td>
	</tr>
	</tbody>
	<tr>
	  <td bgcolor="#FFFFFF">
		<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
			<tr>
				<th align="right">
				<a class="hintstyle" href="javascript:void(0);"  onClick="openHint(1,3);">
				<#WLANConfig11b_x_BRApply_itemname#>
				</a>
				</th>
				<td>
					<input type="radio" value="1" name="rt_wdsapply_x" class="input" onClick="return change_common_radio(this, 'WLANConfig11b', 'rt_wdsapply_x', '1')" <% nvram_match_x("WLANConfig11b","rt_wdsapply_x", "1", "checked"); %>>Yes
					<input type="radio" value="0" name="rt_wdsapply_x" class="input" onClick="return change_common_radio(this, 'WLANConfig11b', 'rt_wdsapply_x', '0')" <% nvram_match_x("WLANConfig11b","rt_wdsapply_x", "0", "checked"); %>>No
				</td>
			</tr>		
			<tr>
				<th align="right" >
				<a class="hintstyle" href="javascript:void(0);"  onClick="openHint(1,1);">
				<#WLANConfig11b_x_APMode_itemname#></a>
				</th>
				<td width="60%">
				  <select name="rt_mode_x" class="input" onChange="return change_common(this, 'WLANConfig11b', 'rt_mode_x');">
					<option value="0" <% nvram_match_x("WLANConfig11b","rt_mode_x", "0","selected"); %>>AP Only</option>
					<option value="1" <% nvram_match_x("WLANConfig11b","rt_mode_x", "1","selected"); %>>WDS Only</option>
					<option value="2" <% nvram_match_x("WLANConfig11b","rt_mode_x", "2","selected"); %>>Hybrid</option>
				  </select>
				</td>
			</tr>
			<tr>
				<th align="right">
				<a class="hintstyle"href="javascript:void(0);"  onClick="openHint(1,2);">
				<#WLANConfig11b_Channel_itemname#>
				</a>
				</th>
				<td><select name="rt_channel" class="input" onChange="return change_common(this, 'WLANConfig11b', 'rt_channel')">
					<% select_channel("WLANConfig11b"); %>
					</select>
				</td>
			</tr>
		</table>
		<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
          <tr>
            <th align="right" id="rt_RBRList"> <#WLANConfig11b_RBRList_groupitemdesc#>
            <td width="60%">
              <input type="text" maxlength="12" class="input" size="14" name="rt_wdslist_x_0" onKeyPress="return is_hwaddr()">
              <input class="button" type="submit" onClick="return markGroup(this, 'rt_RBRList', 2, ' Add ');" name="rt_RBRList" value="<#CTL_add#>" size="12">
              <br/><span>*<#JS_validmac#></span></td>
          </tr>
          <tr>
            <td align="right">&nbsp;</td>
            <td><select size="8" name="rt_RBRList_s" multiple="true" class="input" style="vertical-align:middle; width:150px;">
                <% nvram_get_table_x("WLANConfig11b","rt_RBRList"); %>
              	</select>
                <input class="button" type="submit" onClick="return markGroup(this, 'rt_RBRList', 2, ' Del ');" name="rt_RBRList2" value="<#CTL_del#>" size="12">
            </td>
          </tr>
          <tr align="right">
			<td colspan="2">
				<input type="button" class="button5" value="<#GO_5G#>" onclick="location.href='Advanced_WMode_Content.asp';">    
				<input class="button" onclick="applyRule()" type="button" value="<#CTL_apply#>" /></td>
		  </tr>
        </table></td>
	</tr>
</table>
</td>
</form>
		
	<!--==============Beginning of hint content=============-->
	<td id="help_td" style="width:15px;" valign="top">
<form name="hint_form"></form>
	  <div id="helpicon" onClick="openHint(0,0);">
	  	<img src="images/help.gif" />
	  </div>
	  
	  <div id="hintofPM" style="display:none;">
	    <table width="100%" cellpadding="0" cellspacing="1" class="Help" bgcolor="#999999">
		  <thead>
		  <tr>
			<td>
			  <div id="helpname" class="AiHintTitle"></div>
			  <a href="javascript:;" onclick="closeHint()" ><img src="images/button-close.gif" class="closebutton" /></a>
			</td>
		  </tr>
		  </thead>
		  
		  <tr>				
			<td valign="top" >
			  <div class="hint_body2" id="hint_body"></div>
			  <iframe id="statusframe" name="statusframe" class="statusframe" src="" frameborder="0"></iframe>
			</td>
		  </tr>
		</table>
	  </div>
	</td>
	<!--==============Ending of hint content=============-->
  </tr>
</table>				
<!--===================================Ending of Main Content===========================================-->		
	
	</td>
		
    <td width="10" align="center" valign="top">&nbsp;</td>
	</tr>
</table>

<div id="footer"></div>
</body>
</html>
