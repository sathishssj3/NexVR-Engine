import { Router } from 'express';
import { UserController } from './user.controller.js';
import { authenticate, requireRole } from '../auth/auth.middleware.js';

const router = Router();
const controller = new UserController();

router.use(authenticate);

router.get('/me', controller.getProfile);
router.patch('/tier', requireRole('ADMIN'), controller.updateTier);
router.post('/change-password', controller.changePassword);

export const userRouter = router;
