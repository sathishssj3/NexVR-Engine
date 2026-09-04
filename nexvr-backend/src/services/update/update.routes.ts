import { Router } from 'express';
import { UpdateController } from './update.controller.js';
import { authenticate, requireRole } from '../auth/auth.middleware.js';

const router = Router();
const controller = new UpdateController();

// Public endpoint for launcher/CLI to poll for updates
router.get('/check', controller.check);

// Developer/Admin endpoints for CI/CD artifact publishing
router.get('/upload-url', authenticate, requireRole('DEVELOPER', 'ADMIN'), controller.getUploadPresigned);
router.post('/publish', authenticate, requireRole('DEVELOPER', 'ADMIN'), controller.publish);

export const updateRouter = router;
