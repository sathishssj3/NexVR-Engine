import { Router } from 'express';
import { ProfileController } from './profile.controller.js';
import { authenticate } from '../auth/auth.middleware.js';

const router = Router();
const controller = new ProfileController();

router.get('/game/:gameId', controller.getByGame);
router.get('/:id', controller.getById);

router.post('/', authenticate, controller.create);
router.put('/:id', authenticate, controller.update);
router.delete('/:id', authenticate, controller.delete);

export const profileRouter = router;
