/* F407 回中算法在 C5 上的移植接口，保留原有符号名，方便与旧工程和实车轨迹对照。 */

#include "balance.h"
#include "bsp_system.h"

// 旧版稳球控制的运行状态与参数。

uint8_t balance_state = 0;

uint8_t chose_TI = 2;  //初始状态从第三题开始

/*

测试方法三： 选择速度， 位控电机模式

关键点： 需要读取电机转过的角度值，或者说读取电机转过的脉冲数，这样可以代替读取角度值
		我们一定需要确保，当小球运动到指定目标点的时候杆子回到水平， 也就是说电机此时所有转关的脉冲数之和（正转为正值， 反转为负值）为0；

基础速度 100  clk: 100 - 300
快速 150 clk: 300 -

要是可以控制其再一定够的速度中不抖， 那么我只需要控制电机的位移就行， 偏差小就转小角度， 偏差大就转一个稍微大一些的角度
但是电机抖动如何解决
*/

/*------------------逻辑控制部分-------------------*/

float bias_Kp = 0.0f;
float bias_Kd = 0.0f;

float bias_X = 160.0f;  //由专家指定位置, 对于我们的结构 1cm -> 10个像素点

//第三题, 先不写死
/* 按旧题号选择位置环目标和增益。调试回中使用第 4 组参数，目标为控制坐标 160。 */
 void best_hard(void) {

	 static uint16_t tim = 0;
	 static uint8_t arrive_flag = 0;

	 if (balance_state == 1 && chose_TI == 3) {

		 keep_out.Target = 205.0f;

		 tim++;

		 if (tim >= 300) {

			 keep_out.Kp = 0.01f;
			 keep_inter.Kd = 1.8f;
			  keep_out.Target = 100.0f;
		 }
	 }

	 else if (balance_state == 1 && chose_TI == 4) {

			 keep_out.Kp = 0.08f;
			 keep_inter.Kd = 0.7f;
			 keep_out.Target = 160.0f;

	 }

	 else if (balance_state == 1 && chose_TI == 5) {

		 keep_out.Kp = 0.06f + bias_Kp;		//该偏移量是由第五题巡线时候在转弯处所加0.06
			 keep_inter.Kd = 0.5f + bias_Kd; //0.5
			 keep_out.Target = 160.0f;

	 }

	 else if (balance_state == 1 && chose_TI == 6) {

				keep_out.Target = bias_X;

				if (fabs(keep_out.Actual - bias_X) <= 10.0f) {

						arrive_flag = 1;

				}

				if (arrive_flag == 1) {

						keep_out.Kp = 0.06f + bias_Kp;		//该偏移量是由第五题巡线时候在转弯处所加 0.1
						keep_inter.Kd = 0.5f + bias_Kd; 			//0.4

				}
				else {

					keep_out.Kp = 0.06f + bias_Kp;		//该偏移量是由第五题巡线时候在转弯处所加
					keep_inter.Kd = 0.8f + bias_Kd;

				}

	 }

 }

/*------------------PID调节部分-------------------*/

//内环水管因该倾斜的角度 - 当前水管的角度 -> 电机应该输出的脉冲
//外环， 获取小球当前的位置与目标位置的差距 -> 输出水管应该倾斜的角度

/*-------------外环------------*/
PID_BALANCE keep_out = {

	.Kp = 0.05f,
	.Ki = 0.0f,
	.Kd = 0.4f,  //0.4

	.OutMax = 3.0f,  //18
	.OutMin = -3.0f,  //15*

};

int16_t aim_position = 0;  //水管需要倾斜的角度

//当balance_state == 1的时候进入该PID计算
//1向下运动， 0向上运动
/* 位置外环将钢球坐标误差换算成所需管角，再交给内环。短时丢球时沿用最近实测。 */
void Keep_Blance_out(void) {

	static int16_t last_site = 0;  //存储上一次正确的坐标

	int site_lost = (site.X_0 == 0);  //检测矩形框中心点是否丢失

	//铁球丢失
	if (site_lost == 1) {

	//	keep_out.Target = 200.0f;  //110， 200
		keep_out.Actual = (float)last_site;

		PID_Keep_Blance_out(&keep_out);

		keep_inter.Target = keep_out.Out;  //外环的输出值给到内环的目标值
	}

	//检测到目标坐标点
	else if (site_lost == 0) {

		last_site = site.X_0; //保存正确的铁球坐标

		//keep_out.Target = 200.0f;
		keep_out.Actual = (float)site.X_0;

		PID_Keep_Blance_out(&keep_out);

		keep_inter.Target = keep_out.Out;  //外环的输出值给到内环的目标值

		//加上速度补偿
		//keep_inter.Target = keep_out.Out + 0.5f;
	}

}

/* 按位置误差与球位差分计算倾角，用输出限幅约束外环请求。 */
void PID_Keep_Blance_out(PID_BALANCE* p) {

	p->Error1 = p->Error0;
	p->Error0 = p->Target - p->Actual;

	//p->FeedForward = p->Target - p->Last_Target;  注意符号

			if (fabs(p->Ki) > 0.001) {

				p->ErrorInt += p->Error0;
			}
			else { p->ErrorInt = 0;}

				p->Out = p->Kp * p->Error0
					+ p->Ki * p->ErrorInt
					- p->Kd * (p->Actual - p->Actual1);  // 微分先行：对球位置微分=球速阻尼，在小球越过目标前就开始回调

	if (p->Out > p->OutMax) p->Out = p->OutMax;
	if (p->Out < p->OutMin) p->Out = p->OutMin;

	p->Actual1 = p->Actual;

	p->Last_Target = p->Target;
}

/*------------内环--------------*/

//目的需要内环反应迅速，但是当小球接近目标数值时候 管子目标角度值减小，Kf输出负数来减小管子运动的速度
//Kd也是抑制项， 所以不可用Kf与Kd
//可以考虑Ki， 来确保误差较小时候速度也足够

int16_t speed = 0;

PID_BALANCE keep_inter = {

	.Kf = 0.0f,
	.Kp = 8.0f,   //10
	.Ki = 0.0f,
	.Kd = 0.0f,

	.ErrorIntMax = 0.0f,
	.ErrorIntMin = 0.0f,

	.OutMax = 50.0f,
	.OutMin = -50.0f,

};

//令电机上电时刻位置为0°， 向上运动时候水管实际角度值加加
//向下运动时候水管实际角度值减减
//0向下运动电机实际角度值加加， 1向上运动实际角度值减减

/* 按旧机构的传动比例把轴角换成估计管角，再将内环结果拆成方向和转速。 */
void trace_balance_inter(void) {

	//keep_inter.Target = 2.0f;
	keep_inter.Actual = -(Now_Angle / 360.0f * 3200.0f* 0.039f);   //水管此时的角度, 需要由电机旋转过的脉冲进行转化

	PID_Keep_Blance_inter(&keep_inter);

	speed = (int16_t)keep_inter.Out;

			if (speed >= 0) {

			Contron_X_Speed(1, speed);
		}
		else if (speed < 0) {

			Contron_X_Speed(0, -speed);
		}

}

/* 将管角偏差换成电机转速，在小角差死区内给零速，其余情况按增益和上下限计算。 */
void PID_Keep_Blance_inter(PID_BALANCE* p) {

	p->Error1 = p->Error0;
	p->Error0 = p->Target - p->Actual;

	p->FeedForward = p->Target - p->Last_Target;  //注意符号

	if ( fabs(p->Error0) <= 0.5f) {

		p->Out = 0;
//		Emm_V5_Stop_Now(1, false);  //立即停止

	}

	else {

				if (fabs(p->Ki) > 0.001) {

				//	if (fabs(p->Error0) <=

				p->ErrorInt += p->Error0;
			}
			else { p->ErrorInt = 0;}

				p->Out = p->Kp * p->Error0
					+ p->Ki * p->ErrorInt
					//+ p->Kd * (p->Error0 - p->Error1)
	-p->Kd * (p->Actual - p->Actual1)  //由于电机目标值改变需要迅速抑制所以这里不采用微分先行
					+ p->Kf * p->FeedForward;

	}

	if (p->Out > p->OutMax) p->Out = p->OutMax;
	if (p->Out < p->OutMin) p->Out = p->OutMin;

	p->Actual1 = p->Actual;

	p->Last_Target = p->Target;
}
