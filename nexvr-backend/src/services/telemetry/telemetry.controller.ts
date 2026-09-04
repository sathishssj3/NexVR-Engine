import { Request, Response, NextFunction } from 'express';
import { z } from 'zod';
import { TelemetryService } from './telemetry.service.js';

const telemetryService = new TelemetryService();

const telemetrySchema = z.object({
  clientVersion: z.string().min(1),
  gpuName: z.string().min(1),
  driverVersion: z.string().optional(),
  gameTitle: z.string().optional(),
  targetApi: z.string().optional(),
  frameRate: z.number().optional(),
  frameTimeMs: z.number().optional(),
  isCrash: z.boolean().default(false),
  errorDetails: z.string().optional(),
  stackTrace: z.string().optional(),
});

export class TelemetryController {
  async ingest(req: Request, res: Response, next: NextFunction) {
    try {
      const data = telemetrySchema.parse(req.body);
      const report = await telemetryService.recordReport({
        ...data,
        userId: req.user?.id,
      });
      res.status(201).json({ status: 'success', data: { id: report.id } });
    } catch (err) {
      next(err);
    }
  }

  async getMetrics(_req: Request, res: Response, next: NextFunction) {
    try {
      const summary = await telemetryService.getMetricsSummary();
      res.status(200).json({ status: 'success', data: summary });
    } catch (err) {
      next(err);
    }
  }
}
