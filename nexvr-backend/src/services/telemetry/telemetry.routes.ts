import { Router } from 'express';
import { TelemetryController } from './telemetry.controller.js';
import { authenticate, requireRole } from '../auth/auth.middleware.js';

const router = Router();
const controller = new TelemetryController();

// Anonymous or authenticated telemetry ingestion
router.post('/ingest', controller.ingest);

// Metrics dashboard queries (Admin / Developer only)
router.get('/metrics', authenticate, requireRole('DEVELOPER', 'ADMIN'), controller.getMetrics);

export const telemetryRouter = router;
