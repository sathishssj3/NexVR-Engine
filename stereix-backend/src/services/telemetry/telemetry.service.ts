import { db } from '../../common/db.js';
import { logger } from '../../common/logger.js';

export interface TelemetryPayload {
  userId?: string;
  clientVersion: string;
  gpuName: string;
  driverVersion?: string;
  gameTitle?: string;
  targetApi?: string;
  frameRate?: number;
  frameTimeMs?: number;
  isCrash?: boolean;
  errorDetails?: string;
  stackTrace?: string;
}

export class TelemetryService {
  async recordReport(data: TelemetryPayload) {
    if (data.isCrash) {
      logger.warn(
        {
          version: data.clientVersion,
          gpu: data.gpuName,
          game: data.gameTitle,
          err: data.errorDetails,
        },
        'Crash report received from client'
      );
    }

    return await db.telemetryReport.create({
      data: {
        userId: data.userId,
        clientVersion: data.clientVersion,
        gpuName: data.gpuName,
        driverVersion: data.driverVersion,
        gameTitle: data.gameTitle,
        targetApi: data.targetApi,
        frameRate: data.frameRate,
        frameTimeMs: data.frameTimeMs,
        isCrash: data.isCrash || false,
        errorDetails: data.errorDetails,
        stackTrace: data.stackTrace,
      },
    });
  }

  async getMetricsSummary() {
    const totalReports = await db.telemetryReport.count();
    const totalCrashes = await db.telemetryReport.count({ where: { isCrash: true } });

    const avgPerf = await db.telemetryReport.aggregate({
      where: { isCrash: false },
      _avg: {
        frameRate: true,
        frameTimeMs: true,
      },
    });

    const recentCrashes = await db.telemetryReport.findMany({
      where: { isCrash: true },
      orderBy: { timestamp: 'desc' },
      take: 10,
    });

    return {
      totalReports,
      totalCrashes,
      crashRate: totalReports > 0 ? (totalCrashes / totalReports) * 100 : 0,
      averageFrameRate: avgPerf._avg.frameRate,
      averageFrameTimeMs: avgPerf._avg.frameTimeMs,
      recentCrashes,
    };
  }
}
